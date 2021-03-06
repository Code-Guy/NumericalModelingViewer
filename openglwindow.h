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

//struct Vertex
//{
//    QVector3D position;
//    QVector3D displacement;
//    QVector3D Sig123;
//    QVector4D SigXYZS;
//};

struct Vertex
{
	QVector3D position;
    float totalDeformation;
	QVector3D deformation;
	QVector3D normalElasticStrain;
	QVector3D shearElasticStrain;
    float maximumPrincipalStress;
    float middlePrincipalStress;
    float minimumPrincipalStress;
    QVector3D normalStress;
    QVector3D shearStress;
};

const float kMaxVal = 1e8f;
const float kMinVal = -kMaxVal;
const QVector3D kMaxVec3 = QVector3D(kMaxVal, kMaxVal, kMaxVal);
const QVector3D kMinVec3 = QVector3D(kMinVal, kMinVal, kMinVal);
struct ValueRange
{
	float minTotalDeformation = kMaxVal;
	float maxTotalDeformation = kMinVal;

	QVector3D minDeformation = kMaxVec3;
	QVector3D maxDeformation = kMinVec3;

	QVector3D minNormalElasticStrain = kMaxVec3;
	QVector3D maxNormalElasticStrain = kMinVec3;

	QVector3D minShearElasticStrain = kMaxVec3;
	QVector3D maxShearElasticStrain = kMinVec3;

	float minMaximumPrincipalStress = kMaxVal;
	float maxMaximumPrincipalStress = kMinVal;

	float minMiddlePrincipalStress = kMaxVal;
	float maxMiddlePrincipalStress = kMinVal;

	float minMinimumPrincipalStress = kMaxVal;
	float maxMinimumPrincipalStress = kMinVal;

	QVector3D minNormalStress = kMaxVec3;
	QVector3D maxNormalStress = kMinVec3;

	QVector3D minShearStress = kMaxVec3;
	QVector3D maxShearStress = kMinVec3;
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
    //void loadDataFiles();
    bool loadDatabase();
    void createRenderData();
    QVector3D qMinVec3(const QVector3D& lhs, const QVector3D& rhs);
    QVector3D qMaxVec3(const QVector3D& lhs, const QVector3D& rhs);

    QVector<Vertex> vertices;
    QVector<Zone> zones;
    QVector<Face> faces;
    ValueRange valueRange;
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
