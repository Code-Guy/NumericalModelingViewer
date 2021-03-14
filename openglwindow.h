#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <QOpenGLWidget>
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
#include <mba/mba.hpp>

#include "geoutil.h"

struct NodeVertex
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

enum FacetType
{
	Q4, T3
};

struct Zone
{
    ZoneType type;
    int num;
    quint32 indices[8];
};

struct Facet
{
	FacetType type;
	int num;
	quint32 indices[4];
};

struct UniformGrid
{
	std::array<size_t, 3> dim;
	std::vector<NodeVertex> vertices;
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
    bool loadDatabase();
	void preprocess();
	void clipExteriorMesh();
	void interpUniformGridData();

    QVector3D qMinVec3(const QVector3D& lhs, const QVector3D& rhs);
    QVector3D qMaxVec3(const QVector3D& lhs, const QVector3D& rhs);
    
    template <typename T>
    T qLerp(T a, T b, T t)
	{
		return a * (1 - t) + b * t;
	}

    QVector3D toVec3(const std::array<double, 3>& arr3);
    std::array<double, 3> toArr3(const QVector3D& vec3);

    QVector<NodeVertex> nodeVertices;
    QVector<Facet> facets;

	Mesh mesh;
	Mesh objMesh;
    QVector<Zone> zones;
    ValueRange valueRange;
	QVector<int> zoneTypes;

    BoundingBox boundingBox;
    std::vector<mba::point<3>> coords;
    std::vector<double> values;
	Plane plane;

    QOpenGLShaderProgram* nodeShaderProgram;
    QOpenGLShaderProgram* wireframeShaderProgram;
    QOpenGLShaderProgram* shadedWireframeShaderProgram;
    QOpenGLShaderProgram* sectionShaderProgram;

    QOpenGLBuffer nodeVBO;
    QOpenGLVertexArrayObject nodeVAO;

    QOpenGLVertexArrayObject wireframeVAO;
    QVector<uint32_t> wireframeIndices;
    QOpenGLBuffer wireframeIBO;

	QOpenGLVertexArrayObject zoneVAO;
	QVector<uint32_t> zoneIndices;
	QOpenGLBuffer zoneIBO;

	QOpenGLVertexArrayObject facetVAO;
	QVector<uint32_t> facetIndices;
	QOpenGLBuffer facetIBO;

    QOpenGLBuffer sectionVBO;
	QOpenGLVertexArrayObject sectionVAO;
    QVector<SectionVertex> sectionVertices;
	QVector<uint32_t> sectionIndices;
	QOpenGLBuffer sectionIBO;
	int sectionVertexNum;
	int sectionIndexNum;

	QOpenGLBuffer objVBO;
	QOpenGLVertexArrayObject objVAO;
	QVector<uint32_t> objIndices;
	QOpenGLBuffer objIBO;

    class Camera* camera;
    QTimer updateTimer;
    QElapsedTimer deltaElapsedTimer;
    QElapsedTimer globalElapsedTimer;
    QElapsedTimer profileTimer;
};

#endif // OPENGLWINDOW_H
