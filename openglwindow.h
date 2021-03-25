#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <vector>

#include "geoutil.h"

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
    void loadDataFiles();
    void addFacet(Facet& facet);
    void addZone(Zone& zone);

	void preprocess();
    void clipZones();

    void bindWireframeShaderProgram();
    void bindShadedShaderProgram();

    QVector3D toVec3(const std::array<double, 3>& arr3);
    std::array<double, 3> toArr3(const QVector3D& vec3);

    QVector<NodeVertex> nodeVertices;
    QVector<Facet> facets;

	Mesh mesh;
	Mesh objMesh;
	Plane plane;
    QVector<Zone> zones;
    ValueRange valueRange;
	QVector<int> zoneTypes;

    QOpenGLShaderProgram* wireframeShaderProgram;
	QOpenGLShaderProgram* shadedShaderProgram;

    QOpenGLBuffer nodeVBO;

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
    QOpenGLBuffer sectionIBO;
    QVector<NodeVertex> sectionVertices;
	QVector<uint32_t> sectionIndices;

	QOpenGLVertexArrayObject sectionWireframeVAO;
	QOpenGLBuffer sectionWireframeIBO;
	QVector<uint32_t> sectionWireframeIndices;

	QOpenGLBuffer objVBO;
	QOpenGLVertexArrayObject objVAO;
	QVector<uint32_t> objIndices;
	QOpenGLBuffer objIBO;
	BVHTreeNode* bvhRoot;

    class Camera* camera;
    QTimer updateTimer;
    QElapsedTimer deltaElapsedTimer;
    QElapsedTimer globalElapsedTimer;
    QElapsedTimer profileTimer;

	UniformGrids uniformGrids;
};

#endif // OPENGLWINDOW_H
