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
#include <vector>
#include <array>
#include <mba/mba.hpp>
#include <mpi/mpi.hpp>

typedef MeshPlaneIntersect<double, int> Intersector;

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

struct SectionVertex
{
    QVector3D position;
    QVector3D texcoord;
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

struct BoundingBox
{
    QVector3D min = kMaxVec3;
    QVector3D max = kMinVec3;

    void scale(float s)
    {
        QVector3D offset = (max - min) * (s - 1.0f);
        min -= offset;
        max += offset;
    }
};

struct UniformGrid
{
   std::array<size_t, 3> dim;
   std::vector<Vertex> vertices;
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
    void interpUniformGridData();
    void clipExteriorSurface();

    QVector3D qMinVec3(const QVector3D& lhs, const QVector3D& rhs);
    QVector3D qMaxVec3(const QVector3D& lhs, const QVector3D& rhs);
    
    template <typename T>
    T qLerp(T a, T b, T t)
	{
		return a * (1 - t) + b * t;
	}

    QVector3D toVec3(const std::array<double, 3>& arr3);
    std::array<double, 3> toArr3(const QVector3D& vec3);

    QVector<Vertex> vertices;
    std::vector<Intersector::Vec3D> mpiVertices;
    QVector<Face> faces;
    std::vector<Intersector::Face> mpiFaces;

    QVector<Zone> zones;
    ValueRange valueRange;
	QVector<int> zoneTypes;

    Intersector::Plane plane;
    BoundingBox boundingBox;
    std::vector<std::array<double, 3>> coords;
    std::vector<double> values;

    QOpenGLShaderProgram* nodeShaderProgram;
    QOpenGLShaderProgram* wireframeShaderProgram;
    QOpenGLShaderProgram* shadedWireframeShaderProgram;
    QOpenGLShaderProgram* sectionShaderProgram;

    QOpenGLBuffer nodeVBO;
    QOpenGLVertexArrayObject nodeVAO;

    QOpenGLVertexArrayObject wireframeVAO;
    QVector<GLuint> wireframeIndices;
    QOpenGLBuffer wireframeIBO;

	QOpenGLVertexArrayObject zoneVAO;
	QVector<GLuint> zoneIndices;
	QOpenGLBuffer zoneIBO;

	QOpenGLVertexArrayObject faceVAO;
	QVector<GLuint> faceIndices;
	QOpenGLBuffer faceIBO;

    QOpenGLBuffer sectionVBO;
	QOpenGLVertexArrayObject sectionVAO;
    QVector<SectionVertex> sectionVertices;
	QVector<GLuint> sectionIndices;
	QOpenGLBuffer sectionIBO;

    class Camera* camera;
    QTimer updateTimer;
    QElapsedTimer elapsedTimer;
    QElapsedTimer profileTimer;
};

#endif // OPENGLWINDOW_H
