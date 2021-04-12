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
    void interpUniformGrids();

    void clipZones();
    void genIsosurface(float value);
	void genIsolines(float value);

    void bindPointShaderProgram();
    void bindWireframeShaderProgram();
    void bindShadedShaderProgram();

    QVector<NodeVertex> nodeVertices;
    QVector<Facet> facets;

	Mesh mesh;
	Mesh objMesh;
	Plane plane;
    QVector<Zone> zones;
    ValueRange valueRange;
	QVector<int> zoneTypes;
	BVHTreeNode* zoneBVHRoot;
    BVHTreeNode* faceBVHRoot;
	UniformGrids uniformGrids;

    QOpenGLShaderProgram* pointShaderProgram;
    QOpenGLShaderProgram* wireframeShaderProgram;
	QOpenGLShaderProgram* shadedShaderProgram;

    QOpenGLBuffer nodeVBO;

    QOpenGLBuffer pointVBO;
	QOpenGLVertexArrayObject pointVAO;

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

	QOpenGLBuffer isosurfaceVBO;
	QOpenGLVertexArrayObject isosurfaceVAO;
	QOpenGLBuffer isosurfaceIBO;
	QVector<NodeVertex> isosurfaceVertices;
	QVector<uint32_t> isosurfaceIndices;

	QOpenGLBuffer isolineVBO;
	QOpenGLVertexArrayObject isolineVAO;
	QVector<NodeVertex> isolineVertices;

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
