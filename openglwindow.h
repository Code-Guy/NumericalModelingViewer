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

enum PickMode
{
	PickZone, PickFace, PickNone
};

class OpenGLWindow : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    OpenGLWindow(QWidget *parent);
    ~OpenGLWindow();

    void setDisplayMode(DisplayMode inDisplayMode);
    void setPickMode(PickMode inPickMode);

    void setClipPlane(const Plane& inClipPlane);
    void setDisableClip(bool flag);
	void setIsosurfaceValue(float inIsosurfaceValue);
	void setShowIsosurfaceWireframe(bool flag);
	void setIsolineValue(float inIsolineValue);
    void setShowIsolineWireframe(bool flag);

    QVector2D getIsoValueRange();

    void openFile(const QString& fileName);
    bool exportToEDB(const QString& exportPath);

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
    /** 导入edb格式的数据库文件

		edb数据库文件包括8张表：
            ELEMEDGES表：存储所有网格单元包含的线条信息，每个线条通过两个点索引表征；
			ELEMENTS表：存储所有网格单元节点索引及空间坐标；
			ELETYPE表：存储所有网格单元类型信息；
			EXTERIOR表：存储模型所有外表面对应的节点索引信息；
			FACETS表：存储模型所有表面对应的节点索引信息；
			NODES表：存储节点索引及空间坐标；
			RESULTS表：存储每个节点对应的计算结果值；
			RSTTYPE表：存储计算结果类型、名称；
        
        本接口函数将数据库文件中存储的模型数据和数值计算数据导入到程序内部的Mesh数据结构中

        @param fileName 数据库文件名
        @return 返回是否导入成功
    */
    bool loadDatabase(const QString& fileName);
    bool printDatabase(const QString& fileName);

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
    void bindPickShaderProgram();

    void initResources();
    void cleanResources();

    QVector<NodeVertex> nodeVertices;
    QVector<Facet> exteriorFacets;

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
	QOpenGLShaderProgram* pickShaderProgram;

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

    //QOpenGLBuffer pickVBO;
	QOpenGLVertexArrayObject pickVAO;
	QOpenGLBuffer pickIBO;
    //QVector<NodeVertex> pickVertices;
	QVector<uint32_t> pickIndices;

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
    PickMode pickMode;

    Plane clipPlane;
    bool disableClip;
    float isosurfaceValue;
    bool showIsosurfaceWireframe;
    float isolineValue;
    bool showIsolineWireframe;
};

#endif // OPENGLWINDOW_H
