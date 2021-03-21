#include "openglwindow.h"
#include "camera.h"
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QMessageBox>

OpenGLWindow::OpenGLWindow(QWidget* parent) : QOpenGLWidget(parent)
{
	// 设置窗口属性
	setFocusPolicy(Qt::FocusPolicy::ClickFocus);

	// 加载数据
	loadDatabase();
	preprocess();

	// 初始化摄像机
	camera = new Camera(QVector3D(455.0f, 566.0f, 555.0f), 236.0f, -37.0f, 200.0f, 0.1f);
	camera->setClipping(0.1f, 10000.0f);
	camera->setFovy(60.0f);
}

OpenGLWindow::~OpenGLWindow()
{
	makeCurrent();

	delete camera;

	delete wireframeShaderProgram;
	delete shadedShaderProgram;

	nodeVBO.destroy();
	wireframeVAO.destroy();
	wireframeIBO.destroy();
	zoneVAO.destroy();
	zoneIBO.destroy();
	facetVAO.destroy();
	facetIBO.destroy();
	sectionVAO.destroy();
	sectionVBO.destroy();
	sectionIBO.destroy();
	sectionWireframeVAO.destroy();
	sectionWireframeIBO.destroy();
	objVAO.destroy();
	objVBO.destroy();
	objIBO.destroy();

	doneCurrent();
}

void OpenGLWindow::initializeGL()
{
	initializeOpenGLFunctions();

	// 设置OGL状态
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_LINE_SMOOTH);
	GLint range[2];
	glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
	glLineWidth(range[1]);

	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_PROGRAM_POINT_SIZE);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	// 启用3d纹理
	glEnable(GL_TEXTURE_3D);

	// 创建着色器程序
	{
		wireframeShaderProgram = new QOpenGLShaderProgram;
		bool success = wireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/wireframe_vs.glsl");
		Q_ASSERT_X(success, "wireframeShaderProgram", qPrintable(wireframeShaderProgram->log()));

		success = wireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/wireframe_fs.glsl");
		Q_ASSERT_X(success, "wireframeShaderProgram", qPrintable(wireframeShaderProgram->log()));
		wireframeShaderProgram->link();

		shadedShaderProgram = new QOpenGLShaderProgram;
		success = shadedShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/shaded_vs.glsl");
		Q_ASSERT_X(success, "shadedShaderProgram", qPrintable(shadedShaderProgram->log()));

		success = shadedShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/shaded_fs.glsl");
		Q_ASSERT_X(success, "shadedShaderProgram", qPrintable(shadedShaderProgram->log()));
		shadedShaderProgram->link();
	}

	// 创建基础节点顶点缓存对象
	nodeVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	nodeVBO.create();
	nodeVBO.bind();
	nodeVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	nodeVBO.allocate(nodeVertices.constData(), nodeVertices.count() * sizeof(NodeVertex));

	// 创建线框模式相关渲染资源
	{
		// create the vertex array object
		wireframeVAO.create();
		wireframeVAO.bind();
		nodeVBO.bind();

		// create the index buffer object
		wireframeIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		wireframeIBO.create();
		wireframeIBO.bind();
		wireframeIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		wireframeIBO.allocate(wireframeIndices.constData(), wireframeIndices.count() * sizeof(uint32_t));

		bindWireframeShaderProgram();
	}

	// 创建单元模式相关渲染资源
	{
		zoneVAO.create();
		zoneVAO.bind();
		nodeVBO.bind();

		// create the index buffer object
		zoneIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		zoneIBO.create();
		zoneIBO.bind();
		zoneIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		zoneIBO.allocate(zoneIndices.constData(), zoneIndices.count() * sizeof(uint32_t));
		bindShadedShaderProgram();
	}

	// 创建Face相关渲染资源
	{
		facetVAO.create();
		facetVAO.bind();
		nodeVBO.bind();

		// create the index buffer object
		facetIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		facetIBO.create();
		facetIBO.bind();
		facetIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		facetIBO.allocate(facetIndices.constData(), facetIndices.count() * sizeof(uint32_t));
		bindShadedShaderProgram();
	}

	// 创建截面相关渲染资源
	{
		const int kMaxSectionVertexNum = 5000;
		sectionVertices.resize(kMaxSectionVertexNum);
		sectionIndices.resize(sectionVertices.count() * 10);

		sectionVAO.create();
		sectionVAO.bind();

		sectionVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		sectionVBO.create();
		sectionVBO.bind();
		sectionVBO.setUsagePattern(QOpenGLBuffer::UsagePattern::DynamicDraw);
		sectionVBO.allocate(sectionVertices.constData(), sectionVertices.count() * sizeof(NodeVertex));

		// create the index buffer object
		sectionIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		sectionIBO.create();
		sectionIBO.bind();
		sectionIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
		sectionIBO.allocate(sectionIndices.constData(), sectionIndices.count() * sizeof(uint32_t));
		bindShadedShaderProgram();
		

		// 初始化截面线框资源
		sectionWireframeIndices.resize(sectionVertices.count() * 5);

		sectionWireframeVAO.create();
		sectionWireframeVAO.bind();
		sectionVBO.bind();

		// create the index buffer object
		sectionWireframeIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		sectionWireframeIBO.create();
		sectionWireframeIBO.bind();
		sectionWireframeIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
		sectionWireframeIBO.allocate(sectionWireframeIndices.constData(), sectionWireframeIndices.count() * sizeof(uint32_t));
		bindWireframeShaderProgram();
	}

	// 创建obj模型相关渲染资源
	{
		objVAO.create();
		objVAO.bind();

		objVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		objVBO.create();
		objVBO.bind();
		objVBO.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
		objVBO.allocate(objMesh.vertices.constData(), objMesh.vertices.count() * sizeof(QVector3D));

		// create the index buffer object
		objIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		objIBO.create();
		objIBO.bind();
		objIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		objIBO.allocate(objIndices.constData(), objIndices.count() * sizeof(uint32_t));
	}

	// 初始化计时器
	deltaElapsedTimer.start();
	globalElapsedTimer.start();
	connect(&updateTimer, SIGNAL(timeout()), this, SLOT(update()));
	updateTimer.start(16);
}

void OpenGLWindow::resizeGL(int w, int h)
{
	camera->setAspect((float)w / h);
}

void OpenGLWindow::paintGL()
{
	// 计算deltaTime
	float deltaTime = deltaElapsedTimer.elapsed() * 0.001f;
	deltaElapsedTimer.start();

	// 更新窗口标题，显示帧率
	window()->setWindowTitle(QString("Numerical Modeling Viewer    | %1 FPS").arg((int)(1.0f / deltaTime)));

	// 更新摄像机
	camera->tick(deltaTime);

	// 每帧更新切割面，切割模型
	float globalTime = globalElapsedTimer.elapsed() * 0.001f;
	plane.origin = QVector3D(0.0f, 0.0f, 600.0f * qSin(globalTime));
	plane.normal = QVector3D(-1.0f, -1.0f, -1.0f).normalized();
	plane.dist = QVector3D::dotProduct(plane.origin, plane.normal);
	clipZones();

	glClearColor(0.7, 0.7, 0.7, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	QMatrix4x4 m;
	QMatrix4x4 v = camera->getViewMatrix();
	QMatrix4x4 mv = v * m;
	QMatrix4x4 mvp = camera->getPerspectiveMatrix() * mv;

	wireframeShaderProgram->bind();
	wireframeShaderProgram->setUniformValue("mvp", mvp);
	wireframeShaderProgram->setUniformValue("plane.normal", plane.normal);
	wireframeShaderProgram->setUniformValue("plane.dist", plane.dist);

	wireframeVAO.bind();
	wireframeShaderProgram->setUniformValue("skipClip", false);
	glDrawElements(GL_LINES, wireframeIndices.count(), GL_UNSIGNED_INT, nullptr);

	sectionWireframeVAO.bind();
	wireframeShaderProgram->setUniformValue("skipClip", true);
	glDrawElements(GL_LINES, sectionWireframeIndices.count(), GL_UNSIGNED_INT, nullptr);

	shadedShaderProgram->bind();
	shadedShaderProgram->setUniformValue("mvp", mvp);
	shadedShaderProgram->setUniformValue("mv", mv);
	shadedShaderProgram->setUniformValue("plane.normal", plane.normal);
	shadedShaderProgram->setUniformValue("plane.dist", plane.dist);

	zoneVAO.bind();
	shadedShaderProgram->setUniformValue("skipClip", false);
	glDrawElements(GL_TRIANGLES, zoneIndices.count(), GL_UNSIGNED_INT, nullptr);

	//facetVAO.bind();
	//glDrawElements(GL_TRIANGLES, facetIndices.count(), GL_UNSIGNED_INT, nullptr);

	//objVAO.bind();
	//glDrawElements(GL_TRIANGLES, objIndices.count(), GL_UNSIGNED_INT, nullptr);

	sectionVAO.bind();
	shadedShaderProgram->setUniformValue("skipClip", true);
	glDrawElements(GL_TRIANGLES, sectionIndices.count(), GL_UNSIGNED_INT, nullptr);
}

void OpenGLWindow::mousePressEvent(QMouseEvent* event)
{
	camera->onMousePressed(event->button(), event->x(), event->y());
}

void OpenGLWindow::mouseReleaseEvent(QMouseEvent* event)
{
	camera->onMouseReleased(event->button());
}

void OpenGLWindow::mouseMoveEvent(QMouseEvent* event)
{
	camera->onMouseMoved(event->x(), event->y());
}

void OpenGLWindow::keyPressEvent(QKeyEvent* event)
{
	camera->onKeyPressed(event->key());
}

void OpenGLWindow::keyReleaseEvent(QKeyEvent* event)
{
	camera->onKeyReleased(event->key());
}

bool OpenGLWindow::loadDatabase()
{
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	db.setDatabaseName("asset/data/Hydro.edb");
	if (!db.open()) {
		QMessageBox::critical(nullptr, QObject::tr("Cannot open database"),
			QObject::tr("Unable to establish a database connection.\n"
				"This example needs SQLite support. Please read "
				"the Qt SQL driver documentation for information how "
				"to build it.\n\n"
				"Click Cancel to exit."), QMessageBox::Cancel);
		return false;
	}

	// 查询节点索引及空间坐标
	QSqlQuery query("SELECT * FROM NODES");
	QSqlRecord record = query.record();
	while (query.next())
	{
		NodeVertex nodeVertex;
		for (int i = 0; i < 3; ++i)
		{
			nodeVertex.position[i] = query.value(i + 1).toFloat();
		}

		uniformGrids.bound.min = qMinVec3(uniformGrids.bound.min, nodeVertex.position);
		uniformGrids.bound.max = qMaxVec3(uniformGrids.bound.max, nodeVertex.position);
		mesh.vertices.append(nodeVertex.position);

		nodeVertices.append(nodeVertex);
	}

	// 查询网格单元类型信息
	query.exec("SELECT * FROM ELETYPE");
	record = query.record();
	while (query.next())
	{
		int zoneType = query.value(0).toInt();
		zoneTypes.append(zoneType);
	}

	// 查询网格单元节点索引
	query.exec("SELECT * FROM ELEMENTS");
	record = query.record();

	int index = 0;
	while (query.next())
	{
		Zone zone;
		int type = query.value(1).toInt();
		if (type == 1)
		{
			zone.type = Brick;
			zone.edgeNum = 12;
		}
		else if (type == 2)
		{
			zone.type = Tetrahedron;
			zone.edgeNum = 6;
		}

		zone.vertexNum = query.value(2).toInt();
		for (int i = 0; i < zone.vertexNum; ++i)
		{
			zone.vertices[i] = query.value(i + 3).toInt() - 1;
			zone.bound.combine(nodeVertices[zone.vertices[i]].position);
		}
		zone.bound.centriod = (zone.bound.min + zone.bound.max) * 0.5f;

		if (zone.vertexNum == 8)
		{
			zoneIndices.append({ zone.vertices[0], zone.vertices[3], zone.vertices[1],
				zone.vertices[1], zone.vertices[3], zone.vertices[2],
				zone.vertices[4], zone.vertices[5], zone.vertices[7],
				zone.vertices[5], zone.vertices[6], zone.vertices[7],
				zone.vertices[0], zone.vertices[1], zone.vertices[4],
				zone.vertices[1], zone.vertices[5], zone.vertices[4],
				zone.vertices[1], zone.vertices[2], zone.vertices[5],
				zone.vertices[2], zone.vertices[6], zone.vertices[5],
				zone.vertices[2], zone.vertices[3], zone.vertices[7],
				zone.vertices[2], zone.vertices[7], zone.vertices[6],
				zone.vertices[3], zone.vertices[0], zone.vertices[4],
				zone.vertices[3], zone.vertices[4], zone.vertices[7]
				});

			zone.edges[0] = zone.vertices[0]; 
			zone.edges[1] = zone.vertices[1]; 
			zone.edges[2] = zone.vertices[1]; 
			zone.edges[3] = zone.vertices[2];
			zone.edges[4] = zone.vertices[2]; 
			zone.edges[5] = zone.vertices[3]; 
			zone.edges[6] = zone.vertices[3]; 
			zone.edges[7] = zone.vertices[0];

			zone.edges[8] = zone.vertices[1];
			zone.edges[9] = zone.vertices[5];
			zone.edges[10] = zone.vertices[2];
			zone.edges[11] = zone.vertices[6];
			zone.edges[12] = zone.vertices[3];
			zone.edges[13] = zone.vertices[7];
			zone.edges[14] = zone.vertices[0];
			zone.edges[15] = zone.vertices[4];

			zone.edges[16] = zone.vertices[5];
			zone.edges[17] = zone.vertices[6];
			zone.edges[18] = zone.vertices[6];
			zone.edges[19] = zone.vertices[7];
			zone.edges[20] = zone.vertices[7];
			zone.edges[21] = zone.vertices[4];
			zone.edges[22] = zone.vertices[4];
			zone.edges[23] = zone.vertices[5];
		}
		else if (zone.vertexNum == 4)
		{
			zoneIndices.append({ zone.vertices[0], zone.vertices[1], zone.vertices[3],
				zone.vertices[0], zone.vertices[3], zone.vertices[2],
				zone.vertices[1], zone.vertices[2], zone.vertices[3],
				zone.vertices[0], zone.vertices[2], zone.vertices[1],
				});

			zone.edges[0] = zone.vertices[0];
			zone.edges[1] = zone.vertices[1];
			zone.edges[2] = zone.vertices[0];
			zone.edges[3] = zone.vertices[2];
			zone.edges[4] = zone.vertices[1];
			zone.edges[5] = zone.vertices[2];
			zone.edges[6] = zone.vertices[0];
			zone.edges[7] = zone.vertices[3];
			zone.edges[8] = zone.vertices[1];
			zone.edges[9] = zone.vertices[3];
			zone.edges[10] = zone.vertices[2];
			zone.edges[11] = zone.vertices[3];
		}

		zones.append(zone);
	}

	// 查询网格单元包含的线条信息，每个线条通过两个点索引表征
	query.exec("SELECT * FROM ELEMEDGES");
	record = query.record();
	while (query.next())
	{
		int num = query.value(2).toInt() * 2;
		for (int i = 0; i < num; ++i)
		{
			wireframeIndices.append(query.value(i + 3).toInt() - 1);
		}
	}

	// 查询模型所有表面对应的节点索引信息
	//query.exec("SELECT * FROM FACETS");
	//record = query.record();
	//while (query.next())
	//{
	//	Face facet;
	//	facet.num = query.value(2).toInt();
	//	for (int i = 0; i < facet.num; ++i)
	//	{
	//		facet.indices[i] = query.value(i + 3).toInt() - 1;
	//	}

	//	facets.append(facet);
	//}

	// 查询模型所有外表面对应的节点索引信息
	//query.exec("SELECT * FROM EXTERIOR");
	//record = query.record();
	//while (query.next())
	//{
	//	Facet facet;
	//	facet.num = query.value(3).toInt();
	//	QSet<uint32_t> indexSet;
	//	for (int i = 0; i < facet.num; ++i)
	//	{
	//		facet.indices[i] = query.value(i + 4).toInt() - 1;
	//		indexSet.insert(facet.indices[i]);
	//	}

	//	if (indexSet.count() == 2)
	//	{
	//		continue;
	//	}

	//	if (indexSet.count() < facet.num && indexSet.count() == 3)
	//	{
	//		facet.num = 3;
	//	}
	//	facets.append(facet);

	//	QVector<uint32_t> indices;
	//	if (facet.num == 3)
	//	{
	//		indices.append({ facet.indices[0], facet.indices[1], facet.indices[2] });
	//		wireframeIndices.append({ facet.indices[0], facet.indices[1],
	//			facet.indices[0], facet.indices[2],
	//			facet.indices[1], facet.indices[2] });
	//	}
	//	else if (facet.num == 4)
	//	{
	//		indices.append({
	//			facet.indices[0], facet.indices[1], facet.indices[2],
	//			facet.indices[0], facet.indices[2], facet.indices[3]
	//			});
	//		wireframeIndices.append({ facet.indices[0], facet.indices[1],
	//			facet.indices[1], facet.indices[2],
	//			facet.indices[2], facet.indices[3],
	//			facet.indices[3], facet.indices[0] });
	//	}

	//	facetIndices.append(indices);
	//	for (int i = 0; i < indices.count(); i += 3)
	//	{
	//		uint32_t v0 = indices[i];
	//		uint32_t v1 = indices[i + 1];
	//		uint32_t v2 = indices[i + 2];

	//		GeoUtil::addFace(mesh, v0, v1, v2);
	//	}
	//}

	// 查询每个节点对应的计算结果值
	query.exec("SELECT * FROM RESULTS");
	record = query.record();
	while (query.next())
	{
		int index = query.value(0).toInt() - 1;
		nodeVertices[index].totalDeformation = query.value(1).toFloat();
		valueRange.minTotalDeformation = qMin(valueRange.minTotalDeformation, nodeVertices[index].totalDeformation);
		valueRange.maxTotalDeformation = qMax(valueRange.maxTotalDeformation, nodeVertices[index].totalDeformation);

		int i = 2;
		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].deformation[j] = query.value(i++).toFloat();
		}
		valueRange.minDeformation = qMinVec3(valueRange.minDeformation, nodeVertices[index].deformation);
		valueRange.maxDeformation = qMaxVec3(valueRange.maxDeformation, nodeVertices[index].deformation);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].normalElasticStrain[j] = query.value(i++).toFloat();
		}
		valueRange.minNormalElasticStrain = qMinVec3(valueRange.minNormalElasticStrain, nodeVertices[index].normalElasticStrain);
		valueRange.maxNormalElasticStrain = qMaxVec3(valueRange.maxNormalElasticStrain, nodeVertices[index].normalElasticStrain);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].shearElasticStrain[j] = query.value(i++).toFloat();
		}
		valueRange.minShearElasticStrain = qMinVec3(valueRange.minShearElasticStrain, nodeVertices[index].shearElasticStrain);
		valueRange.maxShearElasticStrain = qMaxVec3(valueRange.maxShearElasticStrain, nodeVertices[index].shearElasticStrain);

		nodeVertices[index].maximumPrincipalStress = query.value(i++).toFloat();
		valueRange.minMaximumPrincipalStress = qMin(valueRange.minMaximumPrincipalStress, nodeVertices[index].maximumPrincipalStress);
		valueRange.maxMaximumPrincipalStress = qMax(valueRange.maxMaximumPrincipalStress, nodeVertices[index].maximumPrincipalStress);

		nodeVertices[index].middlePrincipalStress = query.value(i++).toFloat();
		valueRange.minMiddlePrincipalStress = qMin(valueRange.minMiddlePrincipalStress, nodeVertices[index].middlePrincipalStress);
		valueRange.maxMiddlePrincipalStress = qMax(valueRange.maxMiddlePrincipalStress, nodeVertices[index].middlePrincipalStress);

		nodeVertices[index].minimumPrincipalStress = query.value(i++).toFloat();
		valueRange.minMinimumPrincipalStress = qMin(valueRange.minMinimumPrincipalStress, nodeVertices[index].minimumPrincipalStress);
		valueRange.maxMinimumPrincipalStress = qMax(valueRange.maxMinimumPrincipalStress, nodeVertices[index].minimumPrincipalStress);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].normalStress[j] = query.value(i++).toFloat();
		}
		valueRange.minNormalStress = qMinVec3(valueRange.minNormalStress, nodeVertices[index].normalStress);
		valueRange.maxNormalStress = qMaxVec3(valueRange.maxNormalStress, nodeVertices[index].normalStress);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].shearStress[j] = query.value(i++).toFloat();
		}
		valueRange.minShearStress = qMinVec3(valueRange.minShearStress, nodeVertices[index].shearStress);
		valueRange.maxShearStress = qMaxVec3(valueRange.maxShearStress, nodeVertices[index].shearStress);
	}

	// 查询计算结果类型、名称
	//query.exec("SELECT * FROM RSTTYPE");
	//record = query.record();
	//while (query.next())
	//{
	//	QString str;
	//	for (int i = 0; i < record.count(); ++i)
	//	{
	//		str += query.value(i).toString() + "-";
	//	}

	//	qDebug() << str;
	//}

	return true;
}

void OpenGLWindow::preprocess()
{
	profileTimer.start();

	// 构建bvh树
	bvhRoot = GeoUtil::buildBVHTree(zones);
	qint64 buildTime = profileTimer.restart();

	qint64 clipTime = profileTimer.restart();
}

void OpenGLWindow::clipZones()
{
	profileTimer.start();
	GeoUtil::clipZones(zones, plane, bvhRoot, nodeVertices, sectionVertices, sectionIndices, sectionWireframeIndices);
	
	// 更新缓存数据
	sectionVAO.bind();
	sectionVBO.bind();
	int count = sectionVertices.count() * sizeof(NodeVertex);
	void* dst = sectionVBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)sectionVertices.constData(), count);
	sectionVBO.unmap();

	sectionIBO.bind();
	count = sectionIndices.count() * sizeof(uint32_t);
	dst = sectionIBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)sectionIndices.constData(), count);
	sectionIBO.unmap();

	sectionWireframeVAO.bind();
	sectionWireframeIBO.bind();
	count = sectionWireframeIndices.count() * sizeof(uint32_t);
	dst = sectionWireframeIBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)sectionWireframeIndices.constData(), count);
	sectionWireframeIBO.unmap();

	qint64 clipTime = profileTimer.restart();
	//qDebug() << clipTime;
}

void OpenGLWindow::bindWireframeShaderProgram()
{
	wireframeShaderProgram->bind();
	wireframeShaderProgram->enableAttributeArray(0);
	wireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
}

void OpenGLWindow::bindShadedShaderProgram()
{
	shadedShaderProgram->bind();
	shadedShaderProgram->enableAttributeArray(0);
	shadedShaderProgram->enableAttributeArray(1);
	shadedShaderProgram->enableAttributeArray(2);
	shadedShaderProgram->enableAttributeArray(3);
	shadedShaderProgram->enableAttributeArray(4);
	shadedShaderProgram->enableAttributeArray(5);
	shadedShaderProgram->enableAttributeArray(6);
	shadedShaderProgram->enableAttributeArray(7);
	shadedShaderProgram->enableAttributeArray(8);
	shadedShaderProgram->enableAttributeArray(9);

	shadedShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(NodeVertex, totalDeformation), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(2, GL_FLOAT, offsetof(NodeVertex, deformation), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(3, GL_FLOAT, offsetof(NodeVertex, normalElasticStrain), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(4, GL_FLOAT, offsetof(NodeVertex, shearElasticStrain), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(5, GL_FLOAT, offsetof(NodeVertex, maximumPrincipalStress), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(6, GL_FLOAT, offsetof(NodeVertex, middlePrincipalStress), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(7, GL_FLOAT, offsetof(NodeVertex, minimumPrincipalStress), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(8, GL_FLOAT, offsetof(NodeVertex, normalStress), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(9, GL_FLOAT, offsetof(NodeVertex, shearStress), 3, sizeof(NodeVertex));

	shadedShaderProgram->setUniformValue("valueRange.minTotalDeformation", valueRange.minTotalDeformation);
	shadedShaderProgram->setUniformValue("valueRange.maxTotalDeformation", valueRange.maxTotalDeformation);

	shadedShaderProgram->setUniformValue("valueRange.minDeformation", valueRange.minDeformation);
	shadedShaderProgram->setUniformValue("valueRange.maxDeformation", valueRange.maxDeformation);

	shadedShaderProgram->setUniformValue("valueRange.minNormalElasticStrain", valueRange.minNormalElasticStrain);
	shadedShaderProgram->setUniformValue("valueRange.maxNormalElasticStrain", valueRange.maxNormalElasticStrain);

	shadedShaderProgram->setUniformValue("valueRange.minShearElasticStrain", valueRange.minShearElasticStrain);
	shadedShaderProgram->setUniformValue("valueRange.maxShearElasticStrain", valueRange.maxShearElasticStrain);

	shadedShaderProgram->setUniformValue("valueRange.minMaximumPrincipalStress", valueRange.minMaximumPrincipalStress);
	shadedShaderProgram->setUniformValue("valueRange.maxMaximumPrincipalStress", valueRange.maxMaximumPrincipalStress);

	shadedShaderProgram->setUniformValue("valueRange.minMiddlePrincipalStress", valueRange.minMiddlePrincipalStress);
	shadedShaderProgram->setUniformValue("valueRange.maxMiddlePrincipalStress", valueRange.maxMiddlePrincipalStress);

	shadedShaderProgram->setUniformValue("valueRange.minMinimumPrincipalStress", valueRange.minMinimumPrincipalStress);
	shadedShaderProgram->setUniformValue("maxMinimumPrincipalStress", valueRange.maxMinimumPrincipalStress);

	shadedShaderProgram->setUniformValue("valueRange.minNormalStress", valueRange.minNormalStress);
	shadedShaderProgram->setUniformValue("valueRange.maxNormalStress", valueRange.maxNormalStress);

	shadedShaderProgram->setUniformValue("valueRange.minShearStress", valueRange.minShearStress);
	shadedShaderProgram->setUniformValue("valueRange.maxShearStress", valueRange.maxShearStress);
}

QVector3D OpenGLWindow::toVec3(const std::array<double, 3>& arr3)
{
	return QVector3D(arr3[0], arr3[1], arr3[2]);
}

std::array<double, 3> OpenGLWindow::toArr3(const QVector3D& vec3)
{
	return std::array<double, 3>{vec3[0], vec3[1], vec3[2]};
}