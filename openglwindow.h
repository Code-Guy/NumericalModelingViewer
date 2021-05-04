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

enum DisplayMode
{
    ClipZone, Isosurface, Isoline
};

class OpenGLWindow : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    OpenGLWindow(QWidget *parent);
    ~OpenGLWindow();

    void setDisplayMode(DisplayMode inDisplayMode);
    void setClipPlane(const Plane& inClipPlane);
	void setIsosurfaceValue(float inIsosurfaceValue);
	void setShowIsosurfaceWireframe(bool flag);
	void setIsolineValue(float inIsolineValue);
    void setShowIsolineWireframe(bool flag);

    QVector2D getIsoValueRange();

    void openFile(const QString& fileName);
    void exportToEDB(const QString& exportPath);

signals:
	void onModelStartLoad();
	void onModelFinishLoad();

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
    bool loadDatabase(const QString& fileName);
    void loadDataFiles(const QString& fileName);
    void addFacet(Facet& facet);
    void addZone(Zone& zone);

	void preprocess();
    void interpUniformGrids();

    void clipZones(const Plane& plane);
    void genIsosurface(float value);
	void genIsolines(float value);

    void bindPointShaderProgram();
    void bindWireframeShaderProgram();
    void bindShadedShaderProgram();

    void initResources();
    void cleanResources();

    QVector<NodeVertex> nodeVertices;
    QVector<Facet> facets;

	Mesh mesh;
	Mesh objMesh;
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

    DisplayMode displayMode;
    Plane clipPlane;
    float isosurfaceValue;
    bool showIsosurfaceWireframe;
    float isolineValue;
    bool showIsolineWireframe;
};

#endif // OPENGLWINDOW_H
