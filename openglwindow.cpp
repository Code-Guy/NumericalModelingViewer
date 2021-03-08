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
    //loadDataFiles();
	loadDatabase();
	clipExteriorSurface();
	//interpUniformGridData();

	// 切割
    // 初始化摄像机
	/*camera = new Camera(QVector3D(-30.0f, 72.0f, 72.0f), 308.0f, -30.0f, 20.0f, 0.1f);
	camera->setClipping(0.1f, 1000.0f);*/

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
	faceVAO.destroy();
	faceIBO.destroy();
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
    //glEnable(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_LINE_SMOOTH);
	GLint range[2];
	glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
    glLineWidth(range[1]);

	// 创建着色器程序
    {
		wireframeShaderProgram = new QOpenGLShaderProgram;
		bool success = wireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/wireframe_vs.glsl");
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

		qDebug() << "sectionShaderProgram" << sectionShaderProgram->log();
    }
	
	// create the vertex buffer object
	nodeVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	nodeVBO.create();
	nodeVBO.bind();
	nodeVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	nodeVBO.allocate(vertices.constData(), vertices.count() * sizeof(Vertex));

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
		wireframeIBO.allocate(wireframeIndices.constData(), wireframeIndices.count() * sizeof(GLuint));

		// connect the inputs to the shader program
		wireframeShaderProgram->bind();
		wireframeShaderProgram->enableAttributeArray(0);
		wireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));
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
		zoneIBO.allocate(zoneIndices.constData(), zoneIndices.count() * sizeof(GLuint));

		// connect the inputs to the shader program
		shadedWireframeShaderProgram->bind();
		shadedWireframeShaderProgram->enableAttributeArray(0);
		shadedWireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));
    }

	// 创建Face相关渲染资源
	{
		faceVAO.create();
		faceVAO.bind();
		nodeVBO.bind();

		// create the index buffer object
		faceIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		faceIBO.create();
		faceIBO.bind();
		faceIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		faceIBO.allocate(faceIndices.constData(), faceIndices.count() * sizeof(GLuint));

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

		shadedWireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(Vertex, totalDeformation), 1, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(2, GL_FLOAT, offsetof(Vertex, deformation), 3, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(3, GL_FLOAT, offsetof(Vertex, normalElasticStrain), 3, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(4, GL_FLOAT, offsetof(Vertex, shearElasticStrain), 3, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(5, GL_FLOAT, offsetof(Vertex, maximumPrincipalStress), 1, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(6, GL_FLOAT, offsetof(Vertex, middlePrincipalStress), 1, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(7, GL_FLOAT, offsetof(Vertex, minimumPrincipalStress), 1, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(8, GL_FLOAT, offsetof(Vertex, normalStress), 3, sizeof(Vertex));
		shadedWireframeShaderProgram->setAttributeBuffer(9, GL_FLOAT, offsetof(Vertex, shearStress), 3, sizeof(Vertex));

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

		shadedWireframeShaderProgram->setUniformValue("planeOrigin", toVec3(plane.origin));
		shadedWireframeShaderProgram->setUniformValue("planeNormal", toVec3(plane.normal));
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
		sectionIBO.allocate(sectionIndices.constData(), sectionIndices.count() * sizeof(GLuint));

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

	faceVAO.bind();
	glDrawElements(GL_TRIANGLES, faceIndices.count(), GL_UNSIGNED_INT, nullptr);

	sectionShaderProgram->bind();
	sectionShaderProgram->setUniformValue("mvp", mvp);

	sectionVAO.bind();
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

//void OpenGLWindow::loadDataFiles()
//{
//    // 加载模型网格数据
//	QFile modelFile("asset/data/model001.f3grid");
//	if (modelFile.open(QIODevice::ReadOnly))
//	{
//		QTextStream in(&modelFile);
//		while (!in.atEnd())
//		{
//            QString header;
//            in >> header;
//            if (header == "G")
//            {
//				int index;
//				Vertex vertex;
//				in >> index >> vertex.position[0] >> vertex.position[1] >> vertex.position[2];
//
//				vertices.append(vertex);
//            }
//            else if (header == "Z")
//            {
//                Zone zone;
//                QString type;
//                int Index;
//                in >> type >> Index;
//
//				if (type == "W6")
//				{
//					zone.type = W6;
//					zone.num = 6;
//				}
//                else if (type == "B8")
//                {
//                    zone.type = B8;
//                    zone.num = 8;
//                }
//                else
//                {
//                    qDebug() << "Unknown zone type: " << type;
//                    return;
//                }
//
//                for (int i = 0; i < zone.num; ++i)
//                {
//                    in >> zone.indices[i];
//                    zone.indices[i] -= 1;
//                }
//
//                zones.append(zone);
//            }
//            else if (header == "F")
//            {
//				Face face;
//				QString type;
//				int Index;
//				in >> type >> Index;
//
//				if (type == "Q4")
//				{
//					face.type = Q4;
//					face.num = 4;
//				}
//				else if (type == "T3")
//				{
//					face.type = T3;
//					face.num = 3;
//				}
//				else
//				{
//					qDebug() << "Unknown zone type: " << type;
//					return;
//				}
//
//				for (int i = 0; i < face.num; ++i)
//				{
//					in >> face.indices[i];
//					face.indices[i] -= 1;
//				}
//
//				faces.append(face);
//            }
//			in.readLine();
//		}
//		modelFile.close();
//	}
//
//	// 加载三向（XYZ）位移数据数据
//	QFile gridFile("asset/data/gridpoint_result.txt");
//    if (gridFile.open(QIODevice::ReadOnly))
//    {
//        QTextStream in(&gridFile);
//        in.readLine();
//        while (!in.atEnd())
//        {
//            int index;
//            in >> index;
//            index -= 1;
//
//            in >> vertices[index].displacement[0] >>
//                vertices[index].displacement[1] >>
//                vertices[index].displacement[2];
//            in.readLine();
//        }
//
//        gridFile.close();
//    }
//
//	// 加载应力数据
//	QFile sigForceFile("asset/data/zone_result.txt");
//	if (sigForceFile.open(QIODevice::ReadOnly))
//	{
//		QTextStream in(&sigForceFile);
//		in.readLine();
//		while (!in.atEnd())
//		{
//			int index;
//			in >> index;
//			index -= 1;
//
//			in >> vertices[index].Sig123[0] >>
//				vertices[index].Sig123[1] >>
//				vertices[index].Sig123[2] >>
//				vertices[index].SigXYZS[0] >>
//				vertices[index].SigXYZS[1] >>
//				vertices[index].SigXYZS[2] >>
//				vertices[index].SigXYZS[3];
//			in.readLine();
//		}
//
//        sigForceFile.close();
//	}
//}

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
		Vertex vertex;
		for (int i = 0; i < 3; ++i)
		{
			vertex.position[i] = query.value(i + 1).toFloat();
		}

		boundingBox.min = qMinVec3(boundingBox.min, vertex.position);
		boundingBox.max = qMaxVec3(boundingBox.max, vertex.position);
		coords.push_back(toArr3(vertex.position));
		mpiVertices.push_back(toArr3(vertex.position));

		vertices.append(vertex);
	}

	// 查询模型所有表面对应的节点索引信息
	//query.exec("SELECT * FROM FACETS");
	//record = query.record();
	//while (query.next())
	//{
	//	Face face;
	//	face.num = query.value(2).toInt();
	//	for (int i = 0; i < face.num; ++i)
	//	{
	//		face.indices[i] = query.value(i + 3).toInt() - 1;
	//	}

	//	faces.append(face);
	//}

	// 查询模型所有外表面对应的节点索引信息
	query.exec("SELECT * FROM EXTERIOR");
	record = query.record();
	while (query.next())
	{
		Face face;
		face.num = query.value(3).toInt();
		for (int i = 0; i < face.num; ++i)
		{
			face.indices[i] = query.value(i + 4).toInt() - 1;
		}

		faces.append(face);

		if (face.num == 3)
		{
			faceIndices.append({
				face.indices[0], face.indices[1], face.indices[2]
				});
			mpiFaces.push_back({ (int)face.indices[0], (int)face.indices[1], (int)face.indices[2] });
		}
		else if (face.num == 4)
		{
			faceIndices.append({
				face.indices[0], face.indices[2], face.indices[1],
				face.indices[0], face.indices[3], face.indices[2]
				});
			mpiFaces.push_back({ (int)face.indices[0], (int)face.indices[2], (int)face.indices[1] });
			mpiFaces.push_back({ (int)face.indices[0], (int)face.indices[3], (int)face.indices[2] });
		}
	}

	// 查询每个节点对应的计算结果值
	query.exec("SELECT * FROM RESULTS");
	record = query.record();
	while (query.next())
	{
		int index = query.value(0).toInt() - 1;
		vertices[index].totalDeformation = query.value(1).toFloat();
		valueRange.minTotalDeformation = qMin(valueRange.minTotalDeformation, vertices[index].totalDeformation);
		valueRange.maxTotalDeformation = qMax(valueRange.maxTotalDeformation, vertices[index].totalDeformation);
		values.push_back(vertices[index].totalDeformation);

		int i = 2;
		for (int j = 0; j < 3; ++j)
		{
			vertices[index].deformation[j] = query.value(i++).toFloat();
		}
		valueRange.minDeformation = qMinVec3(valueRange.minDeformation, vertices[index].deformation);
		valueRange.maxDeformation = qMaxVec3(valueRange.maxDeformation, vertices[index].deformation);

		for (int j = 0; j < 3; ++j)
		{
			vertices[index].normalElasticStrain[j] = query.value(i++).toFloat();
		}
		valueRange.minNormalElasticStrain = qMinVec3(valueRange.minNormalElasticStrain, vertices[index].normalElasticStrain);
		valueRange.maxNormalElasticStrain = qMaxVec3(valueRange.maxNormalElasticStrain, vertices[index].normalElasticStrain);

		for (int j = 0; j < 3; ++j)
		{
			vertices[index].shearElasticStrain[j] = query.value(i++).toFloat();
		}
		valueRange.minShearElasticStrain = qMinVec3(valueRange.minShearElasticStrain, vertices[index].shearElasticStrain);
		valueRange.maxShearElasticStrain = qMaxVec3(valueRange.maxShearElasticStrain, vertices[index].shearElasticStrain);

		vertices[index].maximumPrincipalStress = query.value(i++).toFloat();
		valueRange.minMaximumPrincipalStress = qMin(valueRange.minMaximumPrincipalStress, vertices[index].maximumPrincipalStress);
		valueRange.maxMaximumPrincipalStress = qMax(valueRange.maxMaximumPrincipalStress, vertices[index].maximumPrincipalStress);

		vertices[index].middlePrincipalStress = query.value(i++).toFloat();
		valueRange.minMiddlePrincipalStress = qMin(valueRange.minMiddlePrincipalStress, vertices[index].middlePrincipalStress);
		valueRange.maxMiddlePrincipalStress = qMax(valueRange.maxMiddlePrincipalStress, vertices[index].middlePrincipalStress);

		vertices[index].minimumPrincipalStress = query.value(i++).toFloat();
		valueRange.minMinimumPrincipalStress = qMin(valueRange.minMinimumPrincipalStress, vertices[index].minimumPrincipalStress);
		valueRange.maxMinimumPrincipalStress = qMax(valueRange.maxMinimumPrincipalStress, vertices[index].minimumPrincipalStress);

		for (int j = 0; j < 3; ++j)
		{
			vertices[index].normalStress[j] = query.value(i++).toFloat();
		}
		valueRange.minNormalStress = qMinVec3(valueRange.minNormalStress, vertices[index].normalStress);
		valueRange.maxNormalStress = qMaxVec3(valueRange.maxNormalStress, vertices[index].normalStress);

		for (int j = 0; j < 3; ++j)
		{
			vertices[index].shearStress[j] = query.value(i++).toFloat();
		}
		valueRange.minShearStress = qMinVec3(valueRange.minShearStress, vertices[index].shearStress);
		valueRange.maxShearStress = qMaxVec3(valueRange.maxShearStress, vertices[index].shearStress);
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
	mpiVertices.clear();
	mpiFaces.clear();

	double latitudeBands = 30;
	double longitudeBands = 30;
	double radius = 200;
	double pi = 3.1415926;
	for (double latNumber = 0; latNumber <= latitudeBands; latNumber++) {
		double theta = latNumber * pi / latitudeBands;
		double sinTheta = sin(theta);
		double cosTheta = cos(theta);

		for (double longNumber = 0; longNumber <= longitudeBands; longNumber++) {
			double phi = longNumber * 2 * pi / longitudeBands;
			double sinPhi = sin(phi);
			double cosPhi = cos(phi);

			mpiVertices.push_back({ radius * cosPhi * sinTheta, radius * cosTheta, radius * sinPhi * sinTheta });
		}

		for (int latNum = 0; latNum < latitudeBands; latNum++) {
			for (int longNum = 0; longNum < longitudeBands; longNum++) {
				int first = (latNum * (longitudeBands + 1)) + longNum;
				int second = first + longitudeBands + 1;
				mpiFaces.push_back({ first, second, first + 1 });
				mpiFaces.push_back({ second, second + 1, first + 1 });
			}
		}
	}

	Intersector::Mesh mesh(mpiVertices, mpiFaces);
	plane.origin = { 0.0, 0.0, 0.0 };
	plane.normal = toArr3(QVector3D(-1.0, -1.0, -1.0).normalized());
	std::vector<Intersector::Path3D> paths = mesh.Clip(plane);
	//Q_ASSERT_X(paths.size() == 1 && !paths.front().isClosed, "clipExteriorSurface", "path size should be one and not closed!");

	std::vector<Intersector::Path3D> pathss = { paths[0] };
	for (const Intersector::Path3D& path : pathss)
	{
		SectionVertex center;
		center.position = QVector3D(0.0f, 0.0f, 0.0f);
		const auto& points = path.points;
		uint32_t pointNum = (uint32_t)points.size();
		for (size_t i = 0; i < pointNum; ++i)
		{
			center.position += toVec3(points[i]);
		}
		center.position /= pointNum;
		uint32_t baseIndex = sectionVertices.count();
		sectionVertices.append(center);

		for (uint32_t i = 0; i < pointNum; ++i)
		{
			QVector3D current_position = toVec3(points[i]);
			QVector3D next_position = toVec3(points[(i + 1) % pointNum]);

			sectionVertices.append(SectionVertex{ current_position });
			sectionIndices.append({ baseIndex, baseIndex + i + 1, baseIndex + (i + 1) % pointNum + 1 });
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