#include "openglwindow.h"
#include "camera.h"
#include <QDebug>

OpenGLWindow::OpenGLWindow(QWidget *parent) : QOpenGLWidget(parent) 
{
    // store triangle vertex coordinate & color data
    modelVertices = {
        {{ 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }},
        {{ -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }},
        {{ 1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }}};

    // 初始化摄像机
    camera = new Camera(QVector3D(0.0f, 0.0f, 5.0f), 270.0f, 0.0f, 2.0f, 0.1f);
    camera->setFovy(60.0f);
    camera->setClipping(0.1f, 1000.0f);

    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
}

OpenGLWindow::~OpenGLWindow()
{
    makeCurrent();

    delete lineShaderProgram;
    modelVAO.destroy();
    modeVBO.destroy();

    delete camera;

    doneCurrent();
}

void OpenGLWindow::initializeGL() 
{
    initializeOpenGLFunctions();

    // 设置OGL状态
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    // create the shader program
    lineShaderProgram = new QOpenGLShaderProgram;
    bool success = lineShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/simple_vs.glsl");
    success = lineShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/simple_fs.glsl");

    lineShaderProgram->link();
    lineShaderProgram->bind();

    // create the vertex array object
    modelVAO.create();
    modelVAO.bind();

    // create the vertex buffer object
    modeVBO.create();
    modeVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
    modeVBO.bind();
    modeVBO.allocate(modelVertices.constData(), modelVertices.count() * sizeof(ModelVertex));

    // connect the inputs to the shader program
    lineShaderProgram->enableAttributeArray(0);
    lineShaderProgram->enableAttributeArray(1);
    lineShaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(ModelVertex));
    lineShaderProgram->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(GLfloat), 3, sizeof(ModelVertex));

    modelVAO.release();
    modeVBO.release();
	lineShaderProgram->release();

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

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    lineShaderProgram->bind();
    QMatrix4x4 m = camera->getViewPerspectiveMatrix();
    lineShaderProgram->setUniformValue("mvp", m);
    modelVAO.bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    modelVAO.release();
    lineShaderProgram->release();
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