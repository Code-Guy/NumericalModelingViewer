#include "openglwindow.h"
#include "camera.h"
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QMessageBox>

OpenGLWindow::OpenGLWindow(QWidget *parent) : QOpenGLWidget(parent) 
{
    // 设置窗口属性
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    // 加载数据
	loadDatabase();
	preprocess();
	//clipExteriorSurface();
	//interpUniformGridData();

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
    delete shadedWireframeShaderProgram;
	delete sectionShaderProgram;

    nodeVBO.destroy();
	sectionVBO.destroy();
    wireframeVAO.destroy();
    wireframeIBO.destroy();
	zoneVAO.destroy();
	zoneIBO.destroy();
	facetVAO.destroy();
	facetIBO.destroy();
	sectionVAO.destroy();
	sectionIBO.destroy();

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

    glEnable(GL_LINE_SMOOTH);
	GLint range[2];
	glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
    glLineWidth(range[1]);

	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_PROGRAM_POINT_SIZE);

	// 创建着色器程序
    {
		nodeShaderProgram = new QOpenGLShaderProgram;
		bool success = nodeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/node_vs.glsl");
		Q_ASSERT_X(success, "nodeShaderProgram", qPrintable(nodeShaderProgram->log()));

		success = nodeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/node_fs.glsl");
		Q_ASSERT_X(success, "nodeShaderProgram", qPrintable(nodeShaderProgram->log()));
		nodeShaderProgram->link();

		wireframeShaderProgram = new QOpenGLShaderProgram;
		success = wireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/wireframe_vs.glsl");
        Q_ASSERT_X(success, "wireframeShaderProgram", qPrintable(wireframeShaderProgram->log()));

		success = wireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/wireframe_fs.glsl");
        Q_ASSERT_X(success, "wireframeShaderProgram", qPrintable(wireframeShaderProgram->log()));
		wireframeShaderProgram->link();

		shadedWireframeShaderProgram = new QOpenGLShaderProgram;
		success = shadedWireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/shaded_wireframe_vs.glsl");
		Q_ASSERT_X(success, "shadedWireframeShaderProgram", qPrintable(shadedWireframeShaderProgram->log()));

		success = shadedWireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Geometry, "asset/shader/shaded_wireframe_gs.glsl");
		Q_ASSERT_X(success, "shadedWireframeShaderProgram", qPrintable(shadedWireframeShaderProgram->log()));

		success = shadedWireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/shaded_wireframe_fs.glsl");
		Q_ASSERT_X(success, "shadedWireframeShaderProgram", qPrintable(shadedWireframeShaderProgram->log()));
		shadedWireframeShaderProgram->link();

		sectionShaderProgram = new QOpenGLShaderProgram;
		success = sectionShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/section_vs.glsl");
		Q_ASSERT_X(success, "sectionShaderProgram", qPrintable(sectionShaderProgram->log()));

		success = sectionShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/section_fs.glsl");
		Q_ASSERT_X(success, "sectionShaderProgram", qPrintable(sectionShaderProgram->log()));
		sectionShaderProgram->link();
    }
	
	// 创建基础节点顶点缓存对象
	nodeVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	nodeVBO.create();
	nodeVBO.bind();
	nodeVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	nodeVBO.allocate(nodeVertices.constData(), nodeVertices.count() * sizeof(NodeVertex));

	// 创建基础节点相关渲染资源
	{
		nodeVAO.create();
		nodeVAO.bind();
		nodeVBO.bind();

		nodeShaderProgram->bind();
		nodeShaderProgram->enableAttributeArray(0);
		nodeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
	}

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

		// connect the inputs to the shader program
		wireframeShaderProgram->bind();
		wireframeShaderProgram->enableAttributeArray(0);
		wireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
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

		// connect the inputs to the shader program
		shadedWireframeShaderProgram->bind();
		shadedWireframeShaderProgram->enableAttributeArray(0);
		shadedWireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
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

		// connect the inputs to the shader program
		shadedWireframeShaderProgram->bind();
		shadedWireframeShaderProgram->enableAttributeArray(0);
		shadedWireframeShaderProgram->enableAttributeArray(1);
		shadedWireframeShaderProgram->enableAttributeArray(2);
		shadedWireframeShaderProgram->enableAttributeArray(3);
		shadedWireframeShaderProgram->enableAttributeArray(4);
		shadedWireframeShaderProgram->enableAttributeArray(5);
		shadedWireframeShaderProgram->enableAttributeArray(6);
		shadedWireframeShaderProgram->enableAttributeArray(7);
		shadedWireframeShaderProgram->enableAttributeArray(8);
		shadedWireframeShaderProgram->enableAttributeArray(9);

		shadedWireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(NodeVertex, totalDeformation), 1, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(2, GL_FLOAT, offsetof(NodeVertex, deformation), 3, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(3, GL_FLOAT, offsetof(NodeVertex, normalElasticStrain), 3, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(4, GL_FLOAT, offsetof(NodeVertex, shearElasticStrain), 3, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(5, GL_FLOAT, offsetof(NodeVertex, maximumPrincipalStress), 1, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(6, GL_FLOAT, offsetof(NodeVertex, middlePrincipalStress), 1, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(7, GL_FLOAT, offsetof(NodeVertex, minimumPrincipalStress), 1, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(8, GL_FLOAT, offsetof(NodeVertex, normalStress), 3, sizeof(NodeVertex));
		shadedWireframeShaderProgram->setAttributeBuffer(9, GL_FLOAT, offsetof(NodeVertex, shearStress), 3, sizeof(NodeVertex));

		shadedWireframeShaderProgram->setUniformValue("lineWidth", 0.5f);
		shadedWireframeShaderProgram->setUniformValue("lineColor", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));

		shadedWireframeShaderProgram->setUniformValue("minTotalDeformation", valueRange.minTotalDeformation);
		shadedWireframeShaderProgram->setUniformValue("maxTotalDeformation", valueRange.maxTotalDeformation);

		shadedWireframeShaderProgram->setUniformValue("minDeformation", valueRange.minDeformation);
		shadedWireframeShaderProgram->setUniformValue("maxDeformation", valueRange.maxDeformation);

		shadedWireframeShaderProgram->setUniformValue("minNormalElasticStrain", valueRange.minNormalElasticStrain);
		shadedWireframeShaderProgram->setUniformValue("maxNormalElasticStrain", valueRange.maxNormalElasticStrain);

		shadedWireframeShaderProgram->setUniformValue("minShearElasticStrain", valueRange.minShearElasticStrain);
		shadedWireframeShaderProgram->setUniformValue("maxShearElasticStrain", valueRange.maxShearElasticStrain);

		shadedWireframeShaderProgram->setUniformValue("minMaximumPrincipalStress", valueRange.minMaximumPrincipalStress);
		shadedWireframeShaderProgram->setUniformValue("maxMaximumPrincipalStress", valueRange.maxMaximumPrincipalStress);

		shadedWireframeShaderProgram->setUniformValue("minMiddlePrincipalStress", valueRange.minMiddlePrincipalStress);
		shadedWireframeShaderProgram->setUniformValue("maxMiddlePrincipalStress", valueRange.maxMiddlePrincipalStress);

		shadedWireframeShaderProgram->setUniformValue("minMinimumPrincipalStress", valueRange.minMinimumPrincipalStress);
		shadedWireframeShaderProgram->setUniformValue("maxMinimumPrincipalStress", valueRange.maxMinimumPrincipalStress);

		shadedWireframeShaderProgram->setUniformValue("minNormalStress", valueRange.minNormalStress);
		shadedWireframeShaderProgram->setUniformValue("maxNormalStress", valueRange.maxNormalStress);

		shadedWireframeShaderProgram->setUniformValue("minShearStress", valueRange.minShearStress);
		shadedWireframeShaderProgram->setUniformValue("maxShearStress", valueRange.maxShearStress);

		shadedWireframeShaderProgram->setUniformValue("planeOrigin", QVector3D(0.0f, 0.0f, 0.0f));
		shadedWireframeShaderProgram->setUniformValue("planeNormal", QVector3D(-1.0f, -1.0f, -1.0f).normalized());
	}

	// 创建截面相关渲染资源
	{
		sectionVAO.create();
		sectionVAO.bind();

		sectionVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		sectionVBO.create();
		sectionVBO.bind();
		sectionVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		sectionVBO.allocate(sectionVertices.constData(), sectionVertices.count() * sizeof(SectionVertex));

		// create the index buffer object
		sectionIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		sectionIBO.create();
		sectionIBO.bind();
		sectionIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		sectionIBO.allocate(sectionIndices.constData(), sectionIndices.count() * sizeof(uint32_t));

		// connect the inputs to the shader program
		sectionShaderProgram->bind();
		sectionShaderProgram->enableAttributeArray(0);
		sectionShaderProgram->enableAttributeArray(1);
		sectionShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(SectionVertex, position), 3, sizeof(SectionVertex));
		sectionShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(SectionVertex, texcoord), 3, sizeof(SectionVertex));
	}

    // 初始化计时器
    elapsedTimer.start();
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
    float deltaTime = elapsedTimer.elapsed() * 0.001f;
    elapsedTimer.start();
    
    // 更新摄像机
    camera->tick(deltaTime);

    glClearColor(0.7, 0.7, 0.7, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
    QMatrix4x4 m;
    QMatrix4x4 v = camera->getViewMatrix();
    QMatrix4x4 mv = v * m;
    QMatrix4x4 mvp = camera->getPerspectiveMatrix() * mv;

    //wireframeShaderProgram->bind();
    //wireframeShaderProgram->setUniformValue("mvp", mvp);

    //wireframeVAO.bind();
    //glDrawElements(GL_LINES, wireframeIndices.count(), GL_UNSIGNED_INT, nullptr);

    shadedWireframeShaderProgram->bind();
    shadedWireframeShaderProgram->setUniformValue("mvp", mvp);
    shadedWireframeShaderProgram->setUniformValue("mv", mv);
	float halfWidth = width() * 0.5f;
	float halfHeight = height() / 2.0f;
	QMatrix4x4 viewport = viewport = QMatrix4x4(halfWidth, 0.0f, 0.0f, 0.0f,
		0.0f, halfHeight, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		halfWidth + 0, halfHeight + 0, 0.0f, 1.0f);
    shadedWireframeShaderProgram->setUniformValue("viewport", viewport);

	//zoneVAO.bind();
	//glDrawElements(GL_TRIANGLES, zoneIndices.count(), GL_UNSIGNED_INT, nullptr);

	facetVAO.bind();
	glDrawElements(GL_TRIANGLES, facetIndices.count(), GL_UNSIGNED_INT, nullptr);

	sectionShaderProgram->bind();
	sectionShaderProgram->setUniformValue("mvp", mvp);

	sectionVAO.bind();
	glDrawElements(GL_TRIANGLES, sectionIndices.count(), GL_UNSIGNED_INT, nullptr);

	//nodeShaderProgram->bind();
	//nodeShaderProgram->setUniformValue("mvp", mvp);

	//nodeVAO.bind();
	//glDrawArrays(GL_POINTS, 0, nodeVertices.count());
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

	// 查询网格单元类型信息
	QSqlQuery query("SELECT * FROM ELETYPE");
	QSqlRecord record = query.record();
	while (query.next()) 
	{
		int zoneType = query.value(0).toInt();
		zoneTypes.append(zoneType);
	}

	// 查询网格单元节点索引
	query.exec("SELECT * FROM ELEMENTS");
	record = query.record();
	while (query.next()) 
	{
		Zone zone;
		zone.type = (ZoneType)query.value(1).toInt();
		zone.num = query.value(2).toInt();
		for (int i = 0; i < zone.num; ++i)
		{
			zone.indices[i] = query.value(i + 3).toInt() - 1;
		}
		
		zones.append(zone);

		if (zone.num == 8)
		{
			wireframeIndices.append({ zone.indices[0], zone.indices[1], zone.indices[0], zone.indices[2],
				zone.indices[0], zone.indices[3], zone.indices[4], zone.indices[2], zone.indices[4],
				zone.indices[1], zone.indices[4], zone.indices[7], zone.indices[5], zone.indices[2],
				zone.indices[5], zone.indices[3], zone.indices[5], zone.indices[7], zone.indices[6],
				zone.indices[1], zone.indices[6], zone.indices[3], zone.indices[6], zone.indices[7] });

			zoneIndices.append({ zone.indices[0], zone.indices[2], zone.indices[1],
				zone.indices[2], zone.indices[4], zone.indices[1],
				zone.indices[0], zone.indices[3], zone.indices[2],
				zone.indices[3], zone.indices[5], zone.indices[2],
				zone.indices[0], zone.indices[1], zone.indices[3],
				zone.indices[1], zone.indices[6], zone.indices[3],
				zone.indices[2], zone.indices[5], zone.indices[4],
				zone.indices[5], zone.indices[7], zone.indices[4],
				zone.indices[1], zone.indices[4], zone.indices[7],
				zone.indices[1], zone.indices[7], zone.indices[6],
				zone.indices[3], zone.indices[7], zone.indices[5],
				zone.indices[3], zone.indices[6], zone.indices[7]
				});
		}
		else if (zone.num == 6)
		{
			wireframeIndices.append({ zone.indices[0], zone.indices[1], zone.indices[0], zone.indices[2],
				zone.indices[0], zone.indices[3], zone.indices[4], zone.indices[1], zone.indices[4],
				zone.indices[2], zone.indices[4], zone.indices[5], zone.indices[5], zone.indices[2],
				zone.indices[5], zone.indices[3], zone.indices[1], zone.indices[3] });

			zoneIndices.append({ zone.indices[2], zone.indices[5], zone.indices[4],
			zone.indices[0], zone.indices[1], zone.indices[3],
			zone.indices[1], zone.indices[4], zone.indices[3],
			zone.indices[3], zone.indices[4], zone.indices[5],
			zone.indices[0], zone.indices[3], zone.indices[5],
			zone.indices[0], zone.indices[5], zone.indices[2],
			zone.indices[0], zone.indices[2], zone.indices[4],
			zone.indices[0], zone.indices[4], zone.indices[1]
				});
		}
	}
 
	// 查询网格单元包含的线条信息，每个线条通过两个点索引表征
	//query.exec("SELECT * FROM ELEMEDGES");
	//record = query.record();
	//while (query.next())
	//{
	//	int num = query.value(2).toInt() * 2;
	//	for (int i = 0; i < num; ++i)
	//	{
	//		wireframeIndices.append(query.value(i + 3).toInt() - 1);
	//	}
	//}

	// 查询节点索引及空间坐标
	query.exec("SELECT * FROM NODES");
	record = query.record();
	while (query.next())
	{
		NodeVertex nodeVertex;
		for (int i = 0; i < 3; ++i)
		{
			nodeVertex.position[i] = query.value(i + 1).toFloat();
		}

		boundingBox.min = qMinVec3(boundingBox.min, nodeVertex.position);
		boundingBox.max = qMaxVec3(boundingBox.max, nodeVertex.position);
		coords.push_back(toArr3(nodeVertex.position));
		mesh.vertices.append(nodeVertex.position);

		nodeVertices.append(nodeVertex);
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
	query.exec("SELECT * FROM EXTERIOR");
	record = query.record();
	while (query.next())
	{
		Facet facet;
		facet.num = query.value(3).toInt();
		QSet<uint32_t> indexSet;
		for (int i = 0; i < facet.num; ++i)
		{
			facet.indices[i] = query.value(i + 4).toInt() - 1;
			indexSet.insert(facet.indices[i]);
		}

		if (indexSet.count() == 2)
		{
			continue;
		}

		if (indexSet.count() < facet.num && indexSet.count() == 3)
		{
			facet.num = 3;
		}
		facets.append(facet);

		QVector<uint32_t> indices;
		if (facet.num == 3)
		{
			indices.append({ facet.indices[0], facet.indices[1], facet.indices[2] });
		}
		else if (facet.num == 4)
		{
			indices.append({
				facet.indices[0], facet.indices[2], facet.indices[1],
				facet.indices[0], facet.indices[3], facet.indices[2]
				});
		}

		facetIndices.append(indices);
		for (int i = 0; i < indices.count(); i += 3)
		{
			uint32_t v0 = indices[i];
			uint32_t v1 = indices[i + 1];
			uint32_t v2 = indices[i + 2];

			uint32_t faceIndex = mesh.faces.count();
			mesh.faces.append(Face{ v0, v1, v2 });
			Face& face = mesh.faces.back();

			Edge edges[3] = { { v0, v1 }, { v1, v2 }, { v0, v2 } };
			for (int j = 0; j < 3; ++j)
			{
				Edge& edge = edges[j];
				face.edges[j] = edge;
				mesh.edges[edge].append(faceIndex);

				if (edge == Edge{ 13761, 12153 })
				{
					qDebug() << v0 << v1 << v2;
				}
			}
		}
	}

	// 查询每个节点对应的计算结果值
	query.exec("SELECT * FROM RESULTS");
	record = query.record();
	while (query.next())
	{
		int index = query.value(0).toInt() - 1;
		nodeVertices[index].totalDeformation = query.value(1).toFloat();
		valueRange.minTotalDeformation = qMin(valueRange.minTotalDeformation, nodeVertices[index].totalDeformation);
		valueRange.maxTotalDeformation = qMax(valueRange.maxTotalDeformation, nodeVertices[index].totalDeformation);
		values.push_back(nodeVertices[index].totalDeformation);

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
	GeoUtil::fixWindingOrder(mesh);

	facetIndices.resize(mesh.faces.count() * 3);
	for (int i = 0; i < mesh.faces.count(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			facetIndices[i * 3 + j] = mesh.faces[i].vertices[j];
		}
	}

	// 验证模型的封闭性
	bool closed = true;
	int num = 0;
	for (const Face& face : mesh.faces)
	{
		for (const Edge& edge : face.edges)
		{
			const auto& faces = mesh.edges[edge];
			if (faces.count() != 2)
			{
				closed = false;
				num++;
			}
		}
	}

	Q_ASSERT_X(closed, "preprocess", "mesh must be closed!");
}

void OpenGLWindow::interpUniformGridData()
{
	BoundingBox scaledBoundingBox = boundingBox;
	scaledBoundingBox.scale(1.1f);
	mba::point<3> low = toArr3(scaledBoundingBox.min);
	mba::point<3> high = toArr3(scaledBoundingBox.max);

	mba::index<3> dim = { 64, 64, 64 };
	mba::MBA<3> interp(low, high, dim, coords, values);

	UniformGrid uniformGrid;
	uniformGrid.dim = dim;
	uniformGrid.vertices.resize((dim[0] + 1) * (dim[1] + 1) * (dim[2] + 1));
	mba::point<3> min = toArr3(boundingBox.min);
	mba::point<3> max = toArr3(boundingBox.max);

	int index = 0;
	for (int i = 0; i <= dim[0]; ++i)
	{
		for (int j = 0; j <= dim[1]; ++j)
		{
			for (int k = 0; k <= dim[2]; ++k)
			{
				mba::point<3> point;
				point[0] = qLerp<float>(min[0], max[0], (float)i / dim[0]);
				point[1] = qLerp<float>(min[1], max[1], (float)j / dim[1]);
				point[2] = qLerp<float>(min[2], max[2], (float)k / dim[2]);

				double value = interp(point);
				uniformGrid.vertices[index].position = toVec3(point);
				uniformGrid.vertices[index].totalDeformation = value;

				//qDebug() << uniformGrid.vertices[index].position << uniformGrid.vertices[index].totalDeformation;
				index++;
			}
		}
	}
}

void OpenGLWindow::clipExteriorSurface()
{
	QVector<ClipLine> clipLines;
	for (const ClipLine& clipLine : clipLines)
	{
		SectionVertex center;
		center.position = QVector3D(0.0f, 0.0f, 0.0f);
		const auto& vertices = clipLine.vertices;
		uint32_t vertexNum = (uint32_t)vertices.count();
		for (size_t i = 0; i < vertexNum; ++i)
		{
			center.position += vertices[i];
		}
		center.position /= vertexNum;
		uint32_t baseIndex = sectionVertices.count();
		sectionVertices.append(center);

		for (uint32_t i = 0; i < vertexNum; ++i)
		{
			QVector3D current_position = vertices[i];
			QVector3D next_position = vertices[(i + 1) % vertexNum];

			sectionVertices.append(SectionVertex{ current_position });
			sectionIndices.append({ baseIndex, baseIndex + i + 1, baseIndex + (i + 1) % vertexNum + 1 });
		}
	}

	for (SectionVertex& vertex : sectionVertices)
	{
		vertex.texcoord = (vertex.position - boundingBox.min) / (boundingBox.max - boundingBox.min);
	}
}

QVector3D OpenGLWindow::qMinVec3(const QVector3D& lhs, const QVector3D& rhs)
{
	return QVector3D(
		qMin(lhs[0], rhs[0]),
		qMin(lhs[1], rhs[1]),
		qMin(lhs[2], rhs[2])
	);
}

QVector3D OpenGLWindow::qMaxVec3(const QVector3D& lhs, const QVector3D& rhs)
{
	return QVector3D(
		qMax(lhs[0], rhs[0]),
		qMax(lhs[1], rhs[1]),
		qMax(lhs[2], rhs[2])
	);
}

QVector3D OpenGLWindow::toVec3(const std::array<double, 3>& arr3)
{
	return QVector3D(arr3[0], arr3[1], arr3[2]);
}

std::array<double, 3> OpenGLWindow::toArr3(const QVector3D& vec3)
{
	return std::array<double, 3>{vec3[0], vec3[1], vec3[2]};
}