#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QTimer>

struct Vertex
{
    QVector3D position;
    QVector3D displacement;
    QVector3D Sig123;
    QVector4D SigXYZS;
};

enum ZoneType
{
    W6, B8
};

enum FaceType
{
	Q4, T3
};

struct Zone
{
    ZoneType type;
    int num;
    quint32 indices[8];
};

struct Face
{
	FaceType type;
	int num;
	quint32 indices[4];
};

class OpenGLWindow : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    OpenGLWindow(QWidget *parent);
    ~OpenGLWindow();

protected:
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void initializeGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void loadDataFiles();
    bool loadDatabase();
    void createRenderData();

    QVector<Vertex> vertices;
    QVector<Zone> zones;
    QVector<Face> faces;

    QVector<int> zoneTypes;

    QOpenGLShaderProgram* wireframeShaderProgram;
    QOpenGLShaderProgram* shadedWireframeShaderProgram;

    QOpenGLBuffer modelVBO;

    QOpenGLVertexArrayObject wireframeVAO;
    QVector<GLuint> wireframeIndices;
    QOpenGLBuffer wireframeIBO;

	QOpenGLVertexArrayObject zoneVAO;
	QVector<GLuint> zoneIndices;
	QOpenGLBuffer zoneIBO;

	QOpenGLVertexArrayObject faceVAO;
	QVector<GLuint> faceIndices;
	QOpenGLBuffer faceIBO;

    class Camera* camera;
    QTimer updateTimer;
    QElapsedTimer elapsedTimer;
};

#endif // OPENGLWINDOW_H
