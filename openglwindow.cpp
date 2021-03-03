#include "openglwindow.h"
#include "camera.h"
#include <QDebug>
#include <QFile>

OpenGLWindow::OpenGLWindow(QWidget *parent) : QOpenGLWidget(parent) 
{
    // 设置窗口属性
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    // 加载数据
    loadDataFiles();
    createRenderData();

    // 初始化摄像机
    camera = new Camera(QVector3D(-30.0f, 72.0f, 72.0f), 308.0f, -30.0f, 20.0f, 0.1f);
    //camera = new Camera(QVector3D(33.0f, 5.0f, 17.0f), 63.0f, -27.0f, 20.0f, 0.1f);
    //camera = new Camera(QVector3D(0.04f, 2.88f, 2.1f), 301.0f, -62.0f, 20.0f, 0.1f);
    camera->setFovy(60.0f);
    camera->setClipping(0.1f, 1000.0f);
}

OpenGLWindow::~OpenGLWindow()
{
    makeCurrent();

    delete camera;

    delete wireframeShaderProgram;
    delete shadedWireframeShaderProgram;

    modelVBO.destroy();
    wireframeVAO.destroy();
    wireframeIBO.destroy();
	zoneVAO.destroy();
	zoneIBO.destroy();
	faceVAO.destroy();
	faceIBO.destroy();

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
    }
	
	// create the vertex buffer object
	modelVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	modelVBO.create();
	modelVBO.bind();
	modelVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	modelVBO.allocate(modelVertices.constData(), modelVertices.count() * sizeof(ModelVertex));

    // 创建线框模式相关渲染资源
    {
		// create the vertex array object
		wireframeVAO.create();
		wireframeVAO.bind();
		modelVBO.bind();

		// create the index buffer object
		wireframeIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		wireframeIBO.create();
		wireframeIBO.bind();
		wireframeIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		wireframeIBO.allocate(wireframeIndices.constData(), wireframeIndices.count() * sizeof(GLuint));

		// connect the inputs to the shader program
		wireframeShaderProgram->bind();
		wireframeShaderProgram->enableAttributeArray(0);
		wireframeShaderProgram->enableAttributeArray(1);
		wireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(ModelVertex));
		wireframeShaderProgram->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(GLfloat), 3, sizeof(ModelVertex));
    }
    
    // 创建单元模式相关渲染资源
    {
		zoneVAO.create();
		zoneVAO.bind();
		modelVBO.bind();

		// create the index buffer object
		zoneIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		zoneIBO.create();
		zoneIBO.bind();
		zoneIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		zoneIBO.allocate(zoneIndices.constData(), zoneIndices.count() * sizeof(GLuint));

		// connect the inputs to the shader program
		shadedWireframeShaderProgram->bind();
		shadedWireframeShaderProgram->enableAttributeArray(0);
		shadedWireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(ModelVertex));
    }

	// 创建Face相关渲染资源
	{
		faceVAO.create();
		faceVAO.bind();
		modelVBO.bind();

		// create the index buffer object
		faceIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		faceIBO.create();
		faceIBO.bind();
		faceIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		faceIBO.allocate(faceIndices.constData(), faceIndices.count() * sizeof(GLuint));

		// connect the inputs to the shader program
		shadedWireframeShaderProgram->bind();
		shadedWireframeShaderProgram->enableAttributeArray(0);
		shadedWireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(ModelVertex));
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
    shadedWireframeShaderProgram->setUniformValue("LineWidth", 0.5f);
    shadedWireframeShaderProgram->setUniformValue("LineColor", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));

    //zoneVAO.bind();
    //glDrawElements(GL_TRIANGLES, zoneIndices.count(), GL_UNSIGNED_INT, nullptr);

	faceVAO.bind();
	glDrawElements(GL_TRIANGLES, faceIndices.count(), GL_UNSIGNED_INT, nullptr);
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

void OpenGLWindow::loadDataFiles()
{
    // 加载模型网格数据
	QFile modelFile("asset/data/model001.f3grid");
	if (modelFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&modelFile);
		while (!in.atEnd())
		{
            QString header;
            in >> header;
            if (header == "G")
            {
				int index;
				ModelVertex vertex;
				in >> index >> vertex.position[0] >> vertex.position[1] >> vertex.position[2];

				modelVertices.append(vertex);
            }
            else if (header == "Z")
            {
                Zone zone;
                QString type;
                int Index;
                in >> type >> Index;

				if (type == "W6")
				{
					zone.type = W6;
					zone.num = 6;
				}
                else if (type == "B8")
                {
                    zone.type = B8;
                    zone.num = 8;
                }
                else
                {
                    qDebug() << "Unknown zone type: " << type;
                    return;
                }

                for (int i = 0; i < zone.num; ++i)
                {
                    in >> zone.indices[i];
                    zone.indices[i] -= 1;
                }

                zones.append(zone);
            }
            else if (header == "F")
            {
				Face face;
				QString type;
				int Index;
				in >> type >> Index;

				if (type == "Q4")
				{
					face.type = Q4;
					face.num = 4;
				}
				else if (type == "T3")
				{
					face.type = T3;
					face.num = 3;
				}
				else
				{
					qDebug() << "Unknown zone type: " << type;
					return;
				}

				for (int i = 0; i < face.num; ++i)
				{
					in >> face.indices[i];
					face.indices[i] -= 1;
				}

				faces.append(face);
            }
			in.readLine();
		}
		modelFile.close();
	}

	// 加载三向（XYZ）位移数据数据
	QFile gridFile("asset/data/gridpoint_result.txt");
    if (gridFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&gridFile);
        in.readLine();
        while (!in.atEnd())
        {
            int index;
            in >> index;
            index -= 1;

            in >> modelVertices[index].displacement[0] >>
                modelVertices[index].displacement[1] >>
                modelVertices[index].displacement[2];
            in.readLine();
        }

        gridFile.close();
    }

	// 加载应力数据
	QFile sigForceFile("asset/data/zone_result.txt");
	if (sigForceFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&sigForceFile);
		in.readLine();
		while (!in.atEnd())
		{
			int index;
			in >> index;
			index -= 1;

			in >> modelVertices[index].Sig123[0] >>
				modelVertices[index].Sig123[1] >>
				modelVertices[index].Sig123[2] >>
				modelVertices[index].SigXYZS[0] >>
				modelVertices[index].SigXYZS[1] >>
				modelVertices[index].SigXYZS[2] >>
				modelVertices[index].SigXYZS[3];
			in.readLine();
		}

        sigForceFile.close();
	}
}

void OpenGLWindow::createRenderData()
{
    for (const Zone& zone : zones)
    {
        //const Zone& zone = zones[15300];
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

	for (const Face& face : faces)
	{
		if (face.num == 3)
		{
			faceIndices.append({
				face.indices[0], face.indices[2], face.indices[1]
				});
		}
		else if (face.num == 4)
		{
			faceIndices.append({
				face.indices[0], face.indices[3], face.indices[2],
				face.indices[0], face.indices[2], face.indices[1]
				});
		}
	}
}
