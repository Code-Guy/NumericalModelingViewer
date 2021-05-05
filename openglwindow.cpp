#include "openglwindow.h"
#include "camera.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QMessageBox>
#include <fstream>
#include <QRandomGenerator>
#include <dualmc/dualmc.h>

OpenGLWindow::OpenGLWindow(QWidget* parent) : QOpenGLWidget(parent)
{
	// 设置窗口属性
	setFocusPolicy(Qt::FocusPolicy::ClickFocus);

	// 初始化摄像机
	camera = new Camera(QVector3D(837.6f, 574.4f, 982.1f), 233.3f, -24.5f, 240.0f, 0.1f);
	camera->setClipping(0.1f, 10000.0f);
	camera->setFovy(45.0f);

	zoneBVHRoot = nullptr;
	faceBVHRoot = nullptr;

	displayMode = ClipZone;
	pickMode = PickZone;

	disableClip = false;
	showIsosurfaceWireframe = false;
	showIsolineWireframe = false;
}

OpenGLWindow::~OpenGLWindow()
{
	makeCurrent();

	delete camera;
	delete pointShaderProgram;
	delete wireframeShaderProgram;
	delete shadedShaderProgram;

	cleanResources();

	doneCurrent();
}

void OpenGLWindow::setDisplayMode(DisplayMode inDisplayMode)
{
	displayMode = inDisplayMode;
}

void OpenGLWindow::setPickMode(PickMode inPickMode)
{
	pickMode = inPickMode;
}

void OpenGLWindow::setClipPlane(const Plane& inClipPlane)
{
	clipPlane = inClipPlane;
	clipPlane.normalize();
	clipZones(clipPlane);
}

void OpenGLWindow::setDisableClip(bool flag)
{
	disableClip = flag;
}

void OpenGLWindow::setIsosurfaceValue(float inIsosurfaceValue)
{
	isosurfaceValue = inIsosurfaceValue;
	genIsosurface(isosurfaceValue);
}

void OpenGLWindow::setShowIsosurfaceWireframe(bool flag)
{
	showIsosurfaceWireframe = flag;
}

void OpenGLWindow::setIsolineValue(float inIsolineValue)
{
	isolineValue = inIsolineValue;
	genIsolines(isolineValue);
}

void OpenGLWindow::setShowIsolineWireframe(bool flag)
{
	showIsolineWireframe = flag;
}

QVector2D OpenGLWindow::getIsoValueRange()
{
	return QVector2D(valueRange.minTotalDeformation, valueRange.maxTotalDeformation);
}

void OpenGLWindow::openFile(const QString& fileName)
{
	emit onModelStartLoad();
	cleanResources();

	QFileInfo fileInfo(fileName);
	QString suffix = fileInfo.suffix();
	if (suffix == "edb")
	{
		loadDatabase(fileName);
		//printDatabase(fileName);
	}
	else if (suffix == "f3grid")
	{
		loadDataFiles(fileName);
	}

	preprocess();
	initResources();

	// 初始化剖切面
	clipPlane.origin = QVector3D(0.0f, 0.0f, 100.0f);
	clipPlane.normal = QVector3D(0.0f, -2.0f, -1.0f);
	setClipPlane(clipPlane);

	// 初始化等值面/线
	QVector2D isoValueRange = getIsoValueRange();
	float isoValue = (isoValueRange[0] + isoValueRange[1]) * 0.7f;
	setIsosurfaceValue(isoValue);
	setIsolineValue(isoValue);

	emit onModelFinishLoad();
}

bool OpenGLWindow::exportToEDB(const QString& exportPath)
{
	if (zones.empty())
	{
		return false;
	}

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	db.setDatabaseName(exportPath);

	if (!db.open())
	{
		QMessageBox::critical(nullptr, QObject::tr("Cannot open database"),
			QObject::tr("Unable to establish a database connection.\n"
				"This example needs SQLite support. Please read "
				"the Qt SQL driver documentation for information how "
				"to build it.\n\n"
				"Click Cancel to exit."), QMessageBox::Cancel);
		return false;
	}

	QSqlQuery query;
	db.transaction();

	// 创建节点索引及空间坐标表格
	query.exec("create table NODES(ID INTEGER primary key, X REAL, Y REAL, Z REAL)");
	for (int i = 0; i < nodeVertices.size(); ++i)
	{
		const NodeVertex& nodeVertice = nodeVertices[i];
		query.exec(QString("INSERT INTO NODES VALUES(%1, %2, %3, %4)").
			arg(i + 1).
			arg(nodeVertice.position[0]).
			arg(nodeVertice.position[1]).
			arg(nodeVertice.position[2]));
	}

	// 创建每个节点对应的计算结果值表格
	query.exec("create table RESULTS(NODEID INTEGER primary key, \
		USUM REAL, UX REAL, UY REAL, UZ REAL, \
		EPTOX REAL, EPTOY REAL, EPTOZ REAL, \
		EPTOXY REAL, EPTOYZ REAL, EPTOXZ REAL, \
		S1 REAL, S2 REAL, S3 REAL, \
		SX REAL, SY REAL, SZ REAL, \
		SXY REAL, SYZ REAL, SXZ REAL)");
	for (int i = 0; i < nodeVertices.size(); ++i)
	{
		const NodeVertex& nodeVertice = nodeVertices[i];
		query.exec(QString("INSERT INTO RESULTS VALUES(%1, %2, %3, %4, %5, %6, %7,\
			%8, %9, %10, %11, %12, %13, %14, %15, %16, %17, %18, %19, %20)").
			arg(i + 1).
			arg(nodeVertice.totalDeformation).
			arg(nodeVertice.deformation[0]).
			arg(nodeVertice.deformation[1]).
			arg(nodeVertice.deformation[2]).
			arg(nodeVertice.normalElasticStrain[0]).
			arg(nodeVertice.normalElasticStrain[1]).
			arg(nodeVertice.normalElasticStrain[2]).
			arg(nodeVertice.shearElasticStrain[0]).
			arg(nodeVertice.shearElasticStrain[1]).
			arg(nodeVertice.shearElasticStrain[2]).
			arg(nodeVertice.maximumPrincipalStress).
			arg(nodeVertice.middlePrincipalStress).
			arg(nodeVertice.minimumPrincipalStress).
			arg(nodeVertice.normalStress[0]).
			arg(nodeVertice.normalStress[1]).
			arg(nodeVertice.normalStress[2]).
			arg(nodeVertice.shearStress[0]).
			arg(nodeVertice.shearStress[1]).
			arg(nodeVertice.shearStress[2]));
	}

	// 创建网格单元类型信息表格
	query.exec("create table ELETYPE(ID INTEGER primary key, TYPE INTEGER, TYPENAME TEXT)");
	query.exec(QString("INSERT INTO ELETYPE VALUES(1, 0, \"185\")"));
	query.exec(QString("INSERT INTO ELETYPE VALUES(2, 4, \"154\")"));

	// 创建网格单元节点索引表格
	QString elementsTableCreateSql("create table ELEMENTS(ID INTEGER primary key, TYPE INTEGER, NUM INTEGER");
	for (int i = 0; i < 50; ++i)
	{
		elementsTableCreateSql += QString(", N%1 INTEGER").arg(i + 1);
	}
	elementsTableCreateSql += ")";

	query.exec(elementsTableCreateSql);
	for (int i = 0; i < zones.size(); ++i)
	{
		const Zone& zone = zones[i];
		QString elementsTableInsertSql(QString("INSERT INTO ELEMENTS VALUES(%1, %2, %3").
			arg(i + 1).
			arg(zone.type).
			arg(zone.vertexNum));
		static const int reorders[8] = { 0, 1, 4, 2, 3, 6, 7, 5 };
		for (int j = 0; j < 50; ++j)
		{
			int index = 0;
			if (j < zone.vertexNum)
			{
				index = zone.vertices[zone.type == Brick ? reorders[j] : j];
				index++;
			}
			elementsTableInsertSql += QString(", %1").arg(index);
		}
		elementsTableInsertSql += ")";

		query.exec(elementsTableInsertSql);
	}

	// 创建模型所有外表面对应的节点索引信息表格
	QString exteriorTableCreateSql("create table EXTERIOR(ID INT primary key, ELEMID INT, FACETID INT, NUM INT");
	for (int i = 0; i < 50; ++i)
	{
		exteriorTableCreateSql += QString(", N%1 INT").arg(i + 1);
	}
	exteriorTableCreateSql += ")";

	query.exec(exteriorTableCreateSql);
	for (int i = 0; i < exteriorFacets.size(); ++i)
	{
		const Facet& facet = exteriorFacets[i];
		QString exteriorTableInsertSql(QString("INSERT INTO EXTERIOR VALUES(%1, %2, %3, %4").
			arg(i + 1).
			arg(facet.elemID + 1).
			arg(facet.facetID + 1).
			arg(facet.num));
		for (int j = 0; j < 50; ++j)
		{
			exteriorTableInsertSql += QString(", %1").arg(j < facet.num ? facet.indices[j] + 1 : 0);
		}
		exteriorTableInsertSql += ")";

		query.exec(exteriorTableInsertSql);
	}

	// 创建网格单元包含的线条信息，每个线条通过两个点索引表征表格
	QString facetsTableCreateSql("create table FACETS(ID INTEGER primary key, ELEMID INTEGER, NUM INTEGER");
	for (int i = 0; i < 50; ++i)
	{
		facetsTableCreateSql += QString(", N%1 INTEGER").arg(i + 1);
	}
	facetsTableCreateSql += ")";

	query.exec(facetsTableCreateSql);
	int facetID = 0;
	for (const Zone& zone : zones)
	{
		for (const Facet& facet : zone.facets)
		{
			QString facetsTableInsertSql(QString("INSERT INTO FACETS VALUES(%1, %2, %3").
				arg(++facetID).
				arg(facet.elemID + 1).
				arg(facet.num));
			for (int i = 0; i < 50; ++i)
			{
				facetsTableInsertSql += QString(", %1").arg(i < facet.num ? facet.indices[i] + 1 : 0);
			}
			facetsTableInsertSql += ")";

			query.exec(facetsTableInsertSql);
		}
	}

	// 创建模型所有表面对应的节点索引信息表格
	QString elemEdgesTableCreateSql("create table FACETS(ID INTEGER primary key, ELEMID INTEGER, EDGENUM INTEGER");
	for (int i = 0; i < 20; ++i)
	{
		elemEdgesTableCreateSql += QString(", startIdx%1 INTEGER, endIdx1%1 INTEGER").arg(i + 1);
	}
	elemEdgesTableCreateSql += ")";

	query.exec(elemEdgesTableCreateSql);
	for (int i = 0; i < zones.size(); ++i)
	{
		const Zone& zone = zones[i];
		QString elemEdgesTableInsertSql(QString("INSERT INTO FACETS VALUES(%1, %2, %3").
			arg(i + 1).
			arg(i + 1).
			arg(zone.edgeNum));
		for (int j = 0; j < 20; ++j)
		{
			int startIndex = j < zone.edgeNum ? zone.edges[j * 2] + 1 : 0;
			int endIndex = j < zone.edgeNum ? zone.edges[j * 2 + 1] + 1 : 0;
			elemEdgesTableInsertSql += QString(", %1, %2").arg(startIndex, endIndex);
		}
		elemEdgesTableInsertSql += ")";

		query.exec(elemEdgesTableInsertSql);
	}

	// 创建计算结果类型、名称表格
	query.exec("create table RSTTYPE(ID INTEGER primary key, Code TEXT, Name TEXT, Type TEXT, Unit TEXT)");
	query.exec(QString("INSERT INTO RSTTYPE VALUES(1, \"USUM\", \"%1\", \"Total Deformation\", \"m\")").arg(QStringLiteral("HDY_总位移")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(2, \"UX\", \"%1\", \"Directional Deformation(X Axis)\", \"m\")").arg(QStringLiteral("HDY_X方向位移")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(3, \"UY\", \"%1\", \"Directional Deformation(Y Axis)\", \"m\")").arg(QStringLiteral("HDY_Y方向位移")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(4, \"UZ\", \"%1\", \"Directional Deformation(Z Axis)\", \"m\")").arg(QStringLiteral("HDY_Z方向位移")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(5, \"EPTOX\", \"%1\", \"Normal Elastic Strain(X Axis)\", \"m/m\")").arg(QStringLiteral("HDY_X方向正应变")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(6, \"EPTOY\", \"%1\", \"Normal Elastic Strain(Y Axis)\", \"m/m\")").arg(QStringLiteral("HDY_Y方向正应变")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(7, \"EPTOZ\", \"%1\", \"Normal Elastic Strain(Z Axis)\", \"m/m\")").arg(QStringLiteral("HDY_Z方向正应变")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(8, \"EPTOXY\", \"%1\", \"Shear Elastic Strain(XY Plane)\", \"m/m\")").arg(QStringLiteral("HDY_XY平面剪应变")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(9, \"EPTOYZ\", \"%1\", \"Shear Elastic Strain(YZ Plane)\", \"m/m\")").arg(QStringLiteral("HDY_YZ平面剪应变")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(10, \"EPTOXZ\", \"%1\", \"Shear Elastic Strain(XZ Plane)\", \"m/m\")").arg(QStringLiteral("HDY_XZ平面剪应变")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(11, \"S1\", \"%1\", \"Maximum Principal Stress\", \"Pa\")").arg(QStringLiteral("HDY_最大主应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(12, \"S2\", \"%1\", \"Middle Principal Stress\", \"Pa\")").arg(QStringLiteral("HDY_中间主应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(13, \"S3\", \"%1\", \"Minimum Principal Stress\", \"Pa\")").arg(QStringLiteral("HDY_最小主应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(14, \"SX\", \"%1\", \"Normal Stress(X Axis)\", \"Pa\")").arg(QStringLiteral("HDY_X方向正应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(15, \"SY\", \"%1\", \"Normal Stress(Y Axis)\", \"Pa\")").arg(QStringLiteral("HDY_Y方向正应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(16, \"SZ\", \"%1\", \"Normal Stress(Z Axis)\", \"Pa\")").arg(QStringLiteral("HDY_Z方向正应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(17, \"SXY\", \"%1\", \"Shear Stress(XY Plane)\", \"Pa\")").arg(QStringLiteral("HDY_XY平面剪应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(18, \"SYZ\", \"%1\", \"Shear Stress(YZ Plane)\", \"Pa\")").arg(QStringLiteral("HDY_YZ平面剪应力")));
	query.exec(QString("INSERT INTO RSTTYPE VALUES(19, \"SXZ\", \"%1\", \"Shear Stress(XZ Plane)\", \"Pa\")").arg(QStringLiteral("HDY_XZ平面剪应力")));

	db.commit();
	db.close();

	QMessageBox::information(this, QStringLiteral("提示"),
		QStringLiteral("导出成功！"),
		QMessageBox::Ok);
	return true;
}

void OpenGLWindow::initializeGL()
{
	initializeOpenGLFunctions();
	qsrand(QTime(0, 0, 0).secsTo(QTime::currentTime()));

	// 设置OGL状态
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	//glEnable(GL_CULL_FACE);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_LINE_SMOOTH);
	GLint range[2];
	glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
	glLineWidth(range[1]);

	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_PROGRAM_POINT_SIZE);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	// 启用3d纹理
	glEnable(GL_TEXTURE_3D);

	// 创建着色器程序
	{
		pointShaderProgram = new QOpenGLShaderProgram;
		bool success = pointShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/point_vs.glsl");
		Q_ASSERT_X(success, "pointShaderProgram", qPrintable(pointShaderProgram->log()));

		success = pointShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/point_fs.glsl");
		Q_ASSERT_X(success, "pointShaderProgram", qPrintable(pointShaderProgram->log()));
		pointShaderProgram->link();

		wireframeShaderProgram = new QOpenGLShaderProgram;
		success = wireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/wireframe_vs.glsl");
		Q_ASSERT_X(success, "wireframeShaderProgram", qPrintable(wireframeShaderProgram->log()));

		success = wireframeShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/wireframe_fs.glsl");
		Q_ASSERT_X(success, "wireframeShaderProgram", qPrintable(wireframeShaderProgram->log()));
		wireframeShaderProgram->link();

		shadedShaderProgram = new QOpenGLShaderProgram;
		success = shadedShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/shaded_vs.glsl");
		Q_ASSERT_X(success, "shadedShaderProgram", qPrintable(shadedShaderProgram->log()));

		success = shadedShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/shaded_fs.glsl");
		Q_ASSERT_X(success, "shadedShaderProgram", qPrintable(shadedShaderProgram->log()));
		shadedShaderProgram->link();

		pickShaderProgram = new QOpenGLShaderProgram;
		success = pickShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "asset/shader/pick_vs.glsl");
		Q_ASSERT_X(success, "pickShaderProgram", qPrintable(pickShaderProgram->log()));

		success = pickShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "asset/shader/pick_fs.glsl");
		Q_ASSERT_X(success, "pickShaderProgram", qPrintable(pickShaderProgram->log()));
		pickShaderProgram->link();
	}

	// 初始化计时器
	deltaElapsedTimer.start();
	globalElapsedTimer.start();
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
	float deltaTime = deltaElapsedTimer.elapsed() * 0.001f;
	deltaElapsedTimer.start();

	// 更新窗口标题，显示帧率
	window()->setWindowTitle(QString("Numerical Modeling Viewer    | %1 FPS").arg((int)(1.0f / deltaTime)));

	// 更新摄像机
	camera->tick(deltaTime);

	glClearColor(0.7, 0.7, 0.7, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (zones.empty())
	{
		return;
	}

	QMatrix4x4 m;
	m.setToIdentity();
	QMatrix4x4 v = camera->getViewMatrix();
	QMatrix4x4 vp = camera->getViewPerspectiveMatrix();
	QMatrix4x4 mvp = vp * m;

	wireframeShaderProgram->bind();
	wireframeShaderProgram->setUniformValue("mvp", mvp);
	wireframeShaderProgram->setUniformValue("plane.normal", clipPlane.normal);
	wireframeShaderProgram->setUniformValue("plane.dist", clipPlane.dist);

	if (displayMode == ClipZone)
	{
		// 绘制选中线框
		pickShaderProgram->bind();
		pickShaderProgram->setUniformValue("mvp", mvp);
		pickVAO.bind();
		glDrawElements(GL_LINES, pickIndices.count(), GL_UNSIGNED_INT, nullptr);

		wireframeVAO.bind();
		wireframeShaderProgram->bind();
		wireframeShaderProgram->setUniformValue("skipClip", disableClip);
		glDrawElements(GL_LINES, wireframeIndices.count(), GL_UNSIGNED_INT, nullptr);

		if (!disableClip)
		{
			sectionWireframeVAO.bind();
			wireframeShaderProgram->setUniformValue("skipClip", true);
			glDrawElements(GL_LINES, sectionWireframeIndices.count(), GL_UNSIGNED_INT, nullptr);
		}

		shadedShaderProgram->bind();
		shadedShaderProgram->setUniformValue("mvp", mvp);
		shadedShaderProgram->setUniformValue("mv", v * m);
		shadedShaderProgram->setUniformValue("plane.normal", clipPlane.normal);
		shadedShaderProgram->setUniformValue("plane.dist", clipPlane.dist);

		zoneVAO.bind();
		shadedShaderProgram->setUniformValue("skipClip", disableClip);
		glDrawElements(GL_TRIANGLES, zoneIndices.count(), GL_UNSIGNED_INT, nullptr);

		if (!disableClip)
		{
			sectionVAO.bind();
			shadedShaderProgram->setUniformValue("skipClip", true);
			glDrawElements(GL_TRIANGLES, sectionIndices.count(), GL_UNSIGNED_INT, nullptr);
		}
	}
	else if (displayMode == Isosurface)
	{
		if (showIsosurfaceWireframe)
		{
			wireframeShaderProgram->bind();
			wireframeShaderProgram->setUniformValue("mvp", mvp);

			wireframeVAO.bind();
			wireframeShaderProgram->setUniformValue("skipClip", true);
			glDrawElements(GL_LINES, wireframeIndices.count(), GL_UNSIGNED_INT, nullptr);
		}

		pointShaderProgram->bind();
		pointShaderProgram->setUniformValue("mvp", mvp);
		isosurfaceVAO.bind();
		glDrawElements(GL_TRIANGLES, isosurfaceIndices.count(), GL_UNSIGNED_INT, nullptr);
	}
	else if (displayMode == Isoline)
	{
		if (showIsolineWireframe)
		{
			wireframeShaderProgram->bind();
			wireframeShaderProgram->setUniformValue("mvp", mvp);

			wireframeVAO.bind();
			wireframeShaderProgram->setUniformValue("skipClip", true);
			glDrawElements(GL_LINES, wireframeIndices.count(), GL_UNSIGNED_INT, nullptr);
		}

		pointShaderProgram->bind();
		pointShaderProgram->setUniformValue("mvp", mvp);
		isolineVAO.bind();
		glDrawArrays(GL_LINES, 0, isolineVertices.count());
	}

	//pointVAO.bind();
	//glDrawArrays(GL_POINTS, 0, uniformGrids.points.count());

	//facetVAO.bind();
	//glDrawElements(GL_TRIANGLES, facetIndices.count(), GL_UNSIGNED_INT, nullptr);

	//objVAO.bind();
	//glDrawElements(GL_TRIANGLES, objIndices.count(), GL_UNSIGNED_INT, nullptr);
}

void OpenGLWindow::mousePressEvent(QMouseEvent* event)
{
	camera->onMousePressed(event->button(), event->x(), event->y());

	if (event->button() == Qt::LeftButton && displayMode == ClipZone && pickMode != PickNone && !zones.empty())
	{
		QVector3D start = QVector3D(event->x(), height() - event->y() - 1, 0.0f).unproject(camera->getViewMatrix(), camera->getPerspectiveMatrix(), rect());
		QVector3D end = QVector3D(event->x(), height() - event->y() - 1, 1.0f).unproject(camera->getViewMatrix(), camera->getPerspectiveMatrix(), rect());

		Ray pickRay(start, end - start);
		GeoUtil::pickZone(zones, pickRay, zoneBVHRoot, nodeVertices, pickIndices, pickMode == PickZone);

		makeCurrent();
		//pickVertices = { NodeVertex{start}, NodeVertex{end} };
		//pickVAO.bind();
		//pickVBO.bind();
		//int count = pickVertices.count() * sizeof(NodeVertex);
		//void* dst = pickVBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
		//memcpy(dst, (void*)pickVertices.constData(), count);
		//pickVBO.unmap();

		pickVAO.bind();
		pickIBO.bind();
		int count = pickIndices.count() * sizeof(uint32_t);
		void* dst = pickIBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
		memcpy(dst, (void*)pickIndices.constData(), count);
		pickIBO.unmap();
	}
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

bool OpenGLWindow::loadDatabase(const QString& fileName)
{
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	db.setDatabaseName(fileName);
	if (!db.open())
	{
		QMessageBox::critical(nullptr, QObject::tr("Cannot open database"),
			QObject::tr("Unable to establish a database connection.\n"
				"This example needs SQLite support. Please read "
				"the Qt SQL driver documentation for information how "
				"to build it.\n\n"
				"Click Cancel to exit."), QMessageBox::Cancel);
		return false;
	}

	// 查询节点索引及空间坐标
	QSqlQuery query("SELECT * FROM NODES");
	QSqlRecord record = query.record();
	while (query.next())
	{
		NodeVertex nodeVertex;
		for (int i = 0; i < 3; ++i)
		{
			nodeVertex.position[i] = query.value(i + 1).toFloat();
		}

		uniformGrids.bound.min = qMinVec3(uniformGrids.bound.min, nodeVertex.position);
		uniformGrids.bound.max = qMaxVec3(uniformGrids.bound.max, nodeVertex.position);
		mesh.vertices.append(nodeVertex.position);

		nodeVertices.append(nodeVertex);
	}

	// 查询每个节点对应的计算结果值
	query.exec("SELECT * FROM RESULTS");
	record = query.record();
	while (query.next())
	{
		int index = query.value(0).toInt() - 1;
		nodeVertices[index].totalDeformation = query.value(1).toFloat();
		valueRange.minTotalDeformation = qMin(valueRange.minTotalDeformation, nodeVertices[index].totalDeformation);
		valueRange.maxTotalDeformation = qMax(valueRange.maxTotalDeformation, nodeVertices[index].totalDeformation);

		int i = 2;
		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].deformation[j] = query.value(i++).toFloat();
		}
		valueRange.minDeformation = qMinVec3(valueRange.minDeformation, nodeVertices[index].deformation);
		valueRange.maxDeformation = qMaxVec3(valueRange.maxDeformation, nodeVertices[index].deformation);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].normalElasticStrain[j] = query.value(i++).toFloat();
		}
		valueRange.minNormalElasticStrain = qMinVec3(valueRange.minNormalElasticStrain, nodeVertices[index].normalElasticStrain);
		valueRange.maxNormalElasticStrain = qMaxVec3(valueRange.maxNormalElasticStrain, nodeVertices[index].normalElasticStrain);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].shearElasticStrain[j] = query.value(i++).toFloat();
		}
		valueRange.minShearElasticStrain = qMinVec3(valueRange.minShearElasticStrain, nodeVertices[index].shearElasticStrain);
		valueRange.maxShearElasticStrain = qMaxVec3(valueRange.maxShearElasticStrain, nodeVertices[index].shearElasticStrain);

		nodeVertices[index].maximumPrincipalStress = query.value(i++).toFloat();
		valueRange.minMaximumPrincipalStress = qMin(valueRange.minMaximumPrincipalStress, nodeVertices[index].maximumPrincipalStress);
		valueRange.maxMaximumPrincipalStress = qMax(valueRange.maxMaximumPrincipalStress, nodeVertices[index].maximumPrincipalStress);

		nodeVertices[index].middlePrincipalStress = query.value(i++).toFloat();
		valueRange.minMiddlePrincipalStress = qMin(valueRange.minMiddlePrincipalStress, nodeVertices[index].middlePrincipalStress);
		valueRange.maxMiddlePrincipalStress = qMax(valueRange.maxMiddlePrincipalStress, nodeVertices[index].middlePrincipalStress);

		nodeVertices[index].minimumPrincipalStress = query.value(i++).toFloat();
		valueRange.minMinimumPrincipalStress = qMin(valueRange.minMinimumPrincipalStress, nodeVertices[index].minimumPrincipalStress);
		valueRange.maxMinimumPrincipalStress = qMax(valueRange.maxMinimumPrincipalStress, nodeVertices[index].minimumPrincipalStress);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].normalStress[j] = query.value(i++).toFloat();
		}
		valueRange.minNormalStress = qMinVec3(valueRange.minNormalStress, nodeVertices[index].normalStress);
		valueRange.maxNormalStress = qMaxVec3(valueRange.maxNormalStress, nodeVertices[index].normalStress);

		for (int j = 0; j < 3; ++j)
		{
			nodeVertices[index].shearStress[j] = query.value(i++).toFloat();
		}
		valueRange.minShearStress = qMinVec3(valueRange.minShearStress, nodeVertices[index].shearStress);
		valueRange.maxShearStress = qMaxVec3(valueRange.maxShearStress, nodeVertices[index].shearStress);
	}

	// 查询网格单元类型信息
	query.exec("SELECT * FROM ELETYPE");
	record = query.record();
	while (query.next())
	{
		int zoneType = query.value(0).toInt();
		zoneTypes.append(zoneType);
	}

	// 查询网格单元节点索引
	query.exec("SELECT * FROM ELEMENTS");
	record = query.record();
	static const int orders[8] = { 0, 1, 3, 4, 2, 7, 5, 6 };
	while (query.next())
	{
		Zone zone;
		int type = query.value(1).toInt();
		if (type == 1)
		{
			zone.type = Brick;
			zone.edgeNum = 12;
		}
		else if (type == 2)
		{
			zone.type = Tetrahedron;
			zone.edgeNum = 6;
		}

		zone.vertexNum = query.value(2).toInt();
		for (int i = 0; i < zone.vertexNum; ++i)
		{
			int o = zone.type == Brick ? orders[i] : i;
			zone.vertices[i] = query.value(o + 3).toInt() - 1;
		}

		addZone(zone);
	}

	// 查询模型所有外表面对应的节点索引信息
	query.exec("SELECT * FROM EXTERIOR");
	record = query.record();
	while (query.next())
	{
		Facet facet;
		facet.elemID = query.value(1).toInt() - 1;
		facet.facetID = query.value(2).toInt() - 1;
		facet.num = query.value(3).toInt();
		if (facet.num == 4)
		{
			facet.type = Q4;
		}
		else if (facet.num == 3)
		{
			facet.type = T3;
		}

		for (int i = 0; i < facet.num; ++i)
		{
			facet.indices[i] = query.value(i + 4).toInt() - 1;
		}

		addFacet(facet);
	}

	// 查询网格单元包含的线条信息，每个线条通过两个点索引表征
	//query.exec("SELECT * FROM ELEMEDGES");
	//record = query.record();
	//while (query.next())
	//{
	//	int num = query.value(2).toInt() * 2;
	//	for (int i = 0; i < num; ++i)
	//	{
	//		wireframeIndices.append(query.value(i + 3).toInt() - 1);
	//	}
	//}

	// 查询模型所有表面对应的节点索引信息
	//query.exec("SELECT * FROM FACETS");
	//record = query.record();
	//while (query.next())
	//{
	//	Face facet;
	//	facet.num = query.value(2).toInt();
	//	for (int i = 0; i < facet.num; ++i)
	//	{
	//		facet.indices[i] = query.value(i + 3).toInt() - 1;
	//	}

	//	allFacets.append(facet);
	//}

	// 查询计算结果类型、名称
	//query.exec("SELECT * FROM RSTTYPE");
	//record = query.record();
	//while (query.next())
	//{
	//	QString str;
	//	for (int i = 0; i < record.count(); ++i)
	//	{
	//		str += query.value(i).toString() + "-";
	//	}

	//	qDebug() << str;
	//}

	db.close();
	return true;
}

bool OpenGLWindow::printDatabase(const QString& fileName)
{
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	db.setDatabaseName(fileName);
	if (!db.open())
	{
		QMessageBox::critical(nullptr, QObject::tr("Cannot open database"),
			QObject::tr("Unable to establish a database connection.\n"
				"This example needs SQLite support. Please read "
				"the Qt SQL driver documentation for information how "
				"to build it.\n\n"
				"Click Cancel to exit."), QMessageBox::Cancel);
		return false;
	}

	// 打印数据库的表结构
	QStringList tables = db.tables();  //获取数据库中的表
	qDebug() << QString::fromLocal8Bit("表的个数: %1").arg(tables.count()); //打印表的个数
	QStringListIterator itr(tables);
	while (itr.hasNext())
	{
		QString tableName = itr.next().toLocal8Bit();
		qDebug() << QString::fromLocal8Bit("表名: ") + tableName;

		QSqlQuery query(QString("PRAGMA table_info(%1)").arg(tableName));
		while (query.next())
		{
			qDebug() << QString(QString::fromLocal8Bit("字段索引: %1    字段名称: %2    字段类型: %3")).
				arg(query.value(0).toString()).arg(query.value(1).toString()).arg(query.value(2).toString());
		}
	}

	// 打印数据库的部分内容
	const int kMaxRecordNum = 100;
	QSqlQuery query;
	QSqlRecord record;
	QStringList tableNames = { "NODES", "RESULTS", "ELETYPE", "ELEMENTS",
		"EXTERIOR", "ELEMEDGES", "FACETS", "RSTTYPE" };
	for (const QString& tableName : tableNames)
	{
		qDebug().noquote() << QString("\n%1:").arg(tableName);

		int RecordNum = 0;
		query.exec(QString("SELECT * FROM %1").arg(tableName));
		record = query.record();
		while (query.next() && RecordNum++ < kMaxRecordNum)
		{
			QString str;
			for (int i = 0; i < record.count(); ++i)
			{
				str += query.value(i).toString() + "-";
			}

			qDebug() << str;
		}
	}

	db.close();
	return true;
}

void OpenGLWindow::loadDataFiles(const QString& fileName)
{
	// 加载模型网格数据
	QString modelFileName = fileName;
	QFileInfo fileInfo(modelFileName);
	QFile modelFile(modelFileName);
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
				NodeVertex nodeVertex;
				in >> index >> nodeVertex.position[0] >> nodeVertex.position[1] >> nodeVertex.position[2];

				nodeVertex.position *= 8.0f;
				mesh.vertices.append(nodeVertex.position);
				nodeVertices.append(nodeVertex);
			}
			else if (header == "Z")
			{
				Zone zone;
				QString type;
				int Index;
				in >> type >> Index;

				if (type == "W6")
				{
					zone.type = Wedge;
					zone.vertexNum = 6;
					zone.edgeNum = 9;
				}
				else if (type == "B8")
				{
					zone.type = Brick;
					zone.vertexNum = 8;
					zone.edgeNum = 12;
				}
				else
				{
					qDebug() << "Unknown zone type: " << type;
					return;
				}

				for (int i = 0; i < zone.vertexNum; ++i)
				{
					in >> zone.vertices[i];
					zone.vertices[i] -= 1;
				}

				addZone(zone);
			}
			else if (header == "F")
			{
				Facet facet;
				QString type;
				int Index;
				in >> type >> Index;

				if (type == "Q4")
				{
					facet.type = Q4;
					facet.num = 4;
				}
				else if (type == "T3")
				{
					facet.type = T3;
					facet.num = 3;
				}
				else
				{
					qDebug() << "Unknown face type: " << type;
					return;
				}

				for (int i = 0; i < facet.num; ++i)
				{
					in >> facet.indices[i];
					facet.indices[i] -= 1;
				}

				facet.elemID = 0;
				facet.facetID = exteriorFacets.count();
				addFacet(facet);
			}
			in.readLine();
		}
		modelFile.close();
	}

	// 加载三向（XYZ）位移数据数据
	QString gridPointFileName = modelFileName.replace(fileInfo.fileName(), "gridpoint_result.txt");
	QFile gridFile(gridPointFileName);
	if (gridFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&gridFile);
		in.readLine();
		while (!in.atEnd())
		{
			int index;
			in >> index;
			index -= 1;

			in >> nodeVertices[index].deformation[0] >>
				nodeVertices[index].deformation[1] >>
				nodeVertices[index].totalDeformation;

			valueRange.minTotalDeformation = qMin(valueRange.minTotalDeformation, nodeVertices[index].totalDeformation);
			valueRange.maxTotalDeformation = qMax(valueRange.maxTotalDeformation, nodeVertices[index].totalDeformation);
			in.readLine();
		}

		gridFile.close();
	}

	// 加载应力数据
	QString zoneResultFileName = modelFileName.replace(fileInfo.fileName(), "zone_result.txt");
	QFile zoneResultFile(zoneResultFileName);
	if (zoneResultFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&zoneResultFile);
		in.readLine();
		while (!in.atEnd())
		{
			int index;
			in >> index;
			index -= 1;

			in >> nodeVertices[index].normalStress[0] >>
				nodeVertices[index].normalStress[1] >>
				nodeVertices[index].normalStress[2] >>
				nodeVertices[index].shearStress[0] >>
				nodeVertices[index].shearStress[1] >>
				nodeVertices[index].shearStress[2] >>
				nodeVertices[index].maximumPrincipalStress;
			in.readLine();
		}

		zoneResultFile.close();
	}

	// zone的缓存计算
	for (Zone& zone : zones)
	{
		zone.cache(nodeVertices);
	}
}

void OpenGLWindow::addFacet(Facet& facet)
{
	QSet<uint32_t> indexSet;
	for (int i = 0; i < facet.num; ++i)
	{
		indexSet.insert(facet.indices[i]);
	}

	if (indexSet.count() == 2)
	{
		return;
	}

	if (indexSet.count() < facet.num && indexSet.count() == 3)
	{
		facet.num = 3;
	}
	exteriorFacets.append(facet);

	QVector<uint32_t> indices;
	if (facet.num == 3)
	{
		indices.append({ facet.indices[0], facet.indices[1], facet.indices[2] });
	}
	else if (facet.num == 4)
	{
		indices.append({
			facet.indices[0], facet.indices[1], facet.indices[2],
			facet.indices[0], facet.indices[2], facet.indices[3]
			});
	}

	facetIndices.append(indices);
	for (int i = 0; i < indices.count(); i += 3)
	{
		uint32_t v0 = indices[i];
		uint32_t v1 = indices[i + 1];
		uint32_t v2 = indices[i + 2];

		GeoUtil::addFace(mesh, v0, v1, v2);
	}
}

void OpenGLWindow::addZone(Zone& zone)
{
	if (!zone.isValid())
	{
		qDebug() << "Invalid zone!";
		return;
	}

	for (int i = 0; i < zone.vertexNum; ++i)
	{
		zone.bound.combine(nodeVertices[zone.vertices[i]].position);
	}
	zone.bound.cache();

	int elemID = zones.count();
	if (zone.vertexNum == 8)
	{
		zoneIndices.append({ zone.vertices[0], zone.vertices[2], zone.vertices[1],
			zone.vertices[1], zone.vertices[2], zone.vertices[4],
			zone.vertices[0], zone.vertices[3], zone.vertices[2],
			zone.vertices[2], zone.vertices[3], zone.vertices[5],
			zone.vertices[2], zone.vertices[5], zone.vertices[4],
			zone.vertices[4], zone.vertices[5], zone.vertices[7],
			zone.vertices[1], zone.vertices[4], zone.vertices[6],
			zone.vertices[4], zone.vertices[7], zone.vertices[6],
			zone.vertices[0], zone.vertices[1], zone.vertices[3],
			zone.vertices[1], zone.vertices[6], zone.vertices[3],
			zone.vertices[3], zone.vertices[6], zone.vertices[5],
			zone.vertices[6], zone.vertices[7], zone.vertices[5]
			});
		
		zone.facets.append({
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[0], zone.vertices[2], zone.vertices[4], zone.vertices[1]}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[0], zone.vertices[3], zone.vertices[5], zone.vertices[2]}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[2], zone.vertices[5], zone.vertices[7], zone.vertices[4]}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[1], zone.vertices[4], zone.vertices[7], zone.vertices[6]}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[0], zone.vertices[1], zone.vertices[6], zone.vertices[3]}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[3], zone.vertices[6], zone.vertices[7], zone.vertices[5]}}
			});

		zone.edges[0] = zone.vertices[0];
		zone.edges[1] = zone.vertices[1];
		zone.edges[2] = zone.vertices[1];
		zone.edges[3] = zone.vertices[4];
		zone.edges[4] = zone.vertices[4];
		zone.edges[5] = zone.vertices[2];
		zone.edges[6] = zone.vertices[2];
		zone.edges[7] = zone.vertices[0];

		zone.edges[8] = zone.vertices[3];
		zone.edges[9] = zone.vertices[6];
		zone.edges[10] = zone.vertices[6];
		zone.edges[11] = zone.vertices[7];
		zone.edges[12] = zone.vertices[7];
		zone.edges[13] = zone.vertices[5];
		zone.edges[14] = zone.vertices[5];
		zone.edges[15] = zone.vertices[3];

		zone.edges[16] = zone.vertices[0];
		zone.edges[17] = zone.vertices[3];
		zone.edges[18] = zone.vertices[1];
		zone.edges[19] = zone.vertices[6];
		zone.edges[20] = zone.vertices[4];
		zone.edges[21] = zone.vertices[7];
		zone.edges[22] = zone.vertices[2];
		zone.edges[23] = zone.vertices[5];
	}
	else if (zone.vertexNum == 6)
	{
		zoneIndices.append({ zone.vertices[0], zone.vertices[1], zone.vertices[3],
			zone.vertices[2], zone.vertices[5], zone.vertices[4],
			zone.vertices[0], zone.vertices[2], zone.vertices[1],
			zone.vertices[1], zone.vertices[2], zone.vertices[4],
			zone.vertices[1], zone.vertices[5], zone.vertices[3],
			zone.vertices[1], zone.vertices[4], zone.vertices[5],
			zone.vertices[0], zone.vertices[3], zone.vertices[2],
			zone.vertices[2], zone.vertices[3], zone.vertices[5]
			});

		zone.facets.append({
			{FacetType::T3, elemID, Zone::facetID++, 3, {zone.vertices[0], zone.vertices[1], zone.vertices[3], 0}},
			{FacetType::T3, elemID, Zone::facetID++, 3, {zone.vertices[2], zone.vertices[5], zone.vertices[4], 0}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[0], zone.vertices[2], zone.vertices[4], zone.vertices[1]}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[1], zone.vertices[3], zone.vertices[5], zone.vertices[4]}},
			{FacetType::Q4, elemID, Zone::facetID++, 4, {zone.vertices[0], zone.vertices[3], zone.vertices[5], zone.vertices[2]}}
			});

		zone.edges[0] = zone.vertices[0];
		zone.edges[1] = zone.vertices[1];
		zone.edges[2] = zone.vertices[1];
		zone.edges[3] = zone.vertices[3];
		zone.edges[4] = zone.vertices[3];
		zone.edges[5] = zone.vertices[0];
		zone.edges[6] = zone.vertices[2];
		zone.edges[7] = zone.vertices[4];
		zone.edges[8] = zone.vertices[4];
		zone.edges[9] = zone.vertices[5];
		zone.edges[10] = zone.vertices[5];
		zone.edges[11] = zone.vertices[2];
		zone.edges[12] = zone.vertices[1];
		zone.edges[13] = zone.vertices[4];
		zone.edges[14] = zone.vertices[3];
		zone.edges[15] = zone.vertices[5];
		zone.edges[16] = zone.vertices[0];
		zone.edges[17] = zone.vertices[2];
	}
	else if (zone.vertexNum == 4)
	{
		zoneIndices.append({ zone.vertices[0], zone.vertices[1], zone.vertices[3],
			zone.vertices[0], zone.vertices[3], zone.vertices[2],
			zone.vertices[1], zone.vertices[2], zone.vertices[3],
			zone.vertices[0], zone.vertices[2], zone.vertices[1],
			});

		zone.facets.append({
			{FacetType::T3, elemID, Zone::facetID++, 3, {zone.vertices[0], zone.vertices[1], zone.vertices[3], 0}},
			{FacetType::T3, elemID, Zone::facetID++, 3, {zone.vertices[0], zone.vertices[3], zone.vertices[2], 0}},
			{FacetType::T3, elemID, Zone::facetID++, 3, {zone.vertices[1], zone.vertices[2], zone.vertices[3], 0}},
			{FacetType::T3, elemID, Zone::facetID++, 3, {zone.vertices[0], zone.vertices[2], zone.vertices[1], 0}},
			});

		zone.edges[0] = zone.vertices[0];
		zone.edges[1] = zone.vertices[1];
		zone.edges[2] = zone.vertices[0];
		zone.edges[3] = zone.vertices[2];
		zone.edges[4] = zone.vertices[1];
		zone.edges[5] = zone.vertices[2];
		zone.edges[6] = zone.vertices[0];
		zone.edges[7] = zone.vertices[3];
		zone.edges[8] = zone.vertices[1];
		zone.edges[9] = zone.vertices[3];
		zone.edges[10] = zone.vertices[2];
		zone.edges[11] = zone.vertices[3];
	}

	for (int i = 0; i < zone.edgeNum * 2; ++i)
	{
		wireframeIndices.append(zone.edges[i]);
	}

	zone.cache(nodeVertices);
	zones.append(zone);
}

void OpenGLWindow::preprocess()
{
	profileTimer.start();

	// 清洗数据
	GeoUtil::cleanMesh(mesh);

	// 修复缠绕顺序问题
	GeoUtil::fixWindingOrder(mesh);

	// 验证模型的合法性
	bool result = GeoUtil::validateMesh(mesh);
	Q_ASSERT_X(result, "preprocess", "mesh is not valid!");

	// 只保留合法面索引
	facetIndices.resize(mesh.faces.count() * 3);
	for (int i = 0; i < mesh.faces.count(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			facetIndices[i * 3 + j] = mesh.faces[i].vertices[j];
		}
	}
	qint64 cleanTime = profileTimer.restart();
	qDebug() << "clean mesh time:" << cleanTime;

	// 更新外表面的包围盒（使用数字范围替换坐标范围）
	for (Face& face : mesh.faces)
	{
		const NodeVertex& nv0 = nodeVertices[face.vertices[0]];
		const NodeVertex& nv1 = nodeVertices[face.vertices[1]];
		const NodeVertex& nv2 = nodeVertices[face.vertices[2]];

		float minVal = qMin3(nv0.totalDeformation, nv1.totalDeformation, nv2.totalDeformation);
		float maxVal = qMax3(nv0.totalDeformation, nv1.totalDeformation, nv2.totalDeformation);
		face.bound.min = QVector3D(minVal, minVal, minVal);
		face.bound.max = QVector3D(maxVal, maxVal, maxVal);
		face.bound.cache();
	}

	// 构建bvh树
	zoneBVHRoot = GeoUtil::buildBVHTree(zones);
	qint64 buildZoneBVHTreeTime = profileTimer.restart();
	qDebug() << "build zone bvh tree time:" << buildZoneBVHTreeTime;

	faceBVHRoot = GeoUtil::buildBVHTree(mesh);
	qint64 buildFaceBVHTreeTime = profileTimer.restart();
	qDebug() << "build face bvh tree time:" << buildFaceBVHTreeTime;

	// 插值均匀网格
	interpUniformGrids();
	qint64 interpTime = profileTimer.restart();
	qDebug() << "interpolate uniform grids time:" << interpTime;
}

void OpenGLWindow::interpUniformGrids()
{
	const Bound& bound = zoneBVHRoot->bound;
	QVector3D size = bound.size();
	float maxDimVal = qMaxDimVal(size);
	int maxDim = 100;
	std::array<int, 3> dim;
	for (int i = 0; i < 3; ++i)
	{
		dim[i] = size[i] / maxDimVal * maxDim;
	}
	uniformGrids.dim = dim;
	uniformGrids.bound = bound;

	float value;
	for (int x = 0; x < dim[0]; ++x)
	{
		for (int y = 0; y < dim[1]; ++y)
		{
			for (int z = 0; z < dim[2]; ++z)
			{
				QVector3D position;
				position[0] = qLerp(bound.min[0], bound.max[0], (float)x / (dim[0] - 1));
				position[1] = qLerp(bound.min[1], bound.max[1], (float)y / (dim[1] - 1));
				position[2] = qLerp(bound.min[2], bound.max[2], (float)z / (dim[2] - 1));

				if (GeoUtil::interpZones(zones, zoneBVHRoot, position, value))
				{
					uniformGrids.points.append({ position, value });
				}

				uniformGrids.voxelData.append(value);
			}
		}
	}
}

void OpenGLWindow::clipZones(const Plane& plane)
{
	if (zones.empty())
	{
		return;
	}

	profileTimer.start();
	GeoUtil::clipZones(zones, plane, zoneBVHRoot, nodeVertices, sectionVertices, sectionIndices, sectionWireframeIndices);

	// 更新缓存数据
	makeCurrent();
	sectionVAO.bind();
	sectionVBO.bind();
	int count = sectionVertices.count() * sizeof(NodeVertex);
	void* dst = sectionVBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)sectionVertices.constData(), count);
	sectionVBO.unmap();

	sectionIBO.bind();
	count = sectionIndices.count() * sizeof(uint32_t);
	dst = sectionIBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)sectionIndices.constData(), count);
	sectionIBO.unmap();

	sectionWireframeVAO.bind();
	sectionWireframeIBO.bind();
	count = sectionWireframeIndices.count() * sizeof(uint32_t);
	dst = sectionWireframeIBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)sectionWireframeIndices.constData(), count);
	sectionWireframeIBO.unmap();

	qint64 clipTime = profileTimer.restart();
	//qDebug() << "clip zones time:" << clipTime;
}

void OpenGLWindow::genIsosurface(float value)
{
	if (zones.empty())
	{
		return;
	}

	// 构建等值面
	profileTimer.restart();
	dualmc::DualMC<float> builder;
	std::vector<dualmc::Vertex> vertices;
	std::vector<dualmc::Quad> quads;
	builder.build(uniformGrids.voxelData.constData(),
		uniformGrids.dim[2], uniformGrids.dim[1], uniformGrids.dim[0],
		value, false, false, vertices, quads);

	qint64 buildIsosurfaceTime = profileTimer.restart();
	//qDebug() << "build isosurface time:" << buildIsosurfaceTime;

	// 更新顶点缓存和索引缓存
	isosurfaceVertices.clear();
	QVector3D dimVector(uniformGrids.dim[0], uniformGrids.dim[1], uniformGrids.dim[2]);
	for (const auto& vertex : vertices)
	{
		QVector3D position(vertex.z, vertex.y, vertex.x);
		position = qMapClampRange(position, QVector3D(0.0f, 0.0f, 0.0f), dimVector, uniformGrids.bound.min, uniformGrids.bound.max);
		isosurfaceVertices.append({ position, value });
	}

	isosurfaceIndices.clear();
	for (const auto& quad : quads)
	{
		isosurfaceIndices.append({ (uint32_t)quad.i0, (uint32_t)quad.i1, (uint32_t)quad.i2 });
		isosurfaceIndices.append({ (uint32_t)quad.i0, (uint32_t)quad.i2, (uint32_t)quad.i3 });
	}

	// 更新GPU缓存资源
	makeCurrent();
	isosurfaceVAO.bind();
	isosurfaceVBO.bind();
	int count = isosurfaceVertices.count() * sizeof(NodeVertex);
	void* dst = isosurfaceVBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)isosurfaceVertices.constData(), count);
	isosurfaceVBO.unmap();

	isosurfaceIBO.bind();
	count = isosurfaceIndices.count() * sizeof(uint32_t);
	dst = isosurfaceIBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)isosurfaceIndices.constData(), count);
	isosurfaceIBO.unmap();

	qint64 uploadTime = profileTimer.restart();
	//qDebug() << "upload isosurface buffer time:" << uploadTime;
}

void OpenGLWindow::genIsolines(float value)
{
	if (zones.empty())
	{
		return;
	}

	// 计算等值线
	isolineVertices.clear();
	QVector<ClipLine> clipLines;
	clipLines = GeoUtil::genIsolines(mesh, nodeVertices, value, faceBVHRoot);

	for (const ClipLine& clipLine : clipLines)
	{
		const QList<QVector3D>& vertices = clipLine.vertices;
		for (int i = 0; i < vertices.count() - 1; ++i)
		{
			isolineVertices.append({ NodeVertex{vertices[i], value}, NodeVertex{vertices[i + 1], value} });
		}
	}

	// 更新缓存数据
	makeCurrent();
	isolineVBO.bind();
	int count = isolineVertices.count() * sizeof(NodeVertex);
	void* dst = isolineVBO.mapRange(0, count, QOpenGLBuffer::RangeAccessFlag::RangeWrite);
	memcpy(dst, (void*)isolineVertices.constData(), count);
	isolineVBO.unmap();
}

void OpenGLWindow::bindPointShaderProgram()
{
	pointShaderProgram->bind();
	pointShaderProgram->enableAttributeArray(0);
	pointShaderProgram->enableAttributeArray(1);
	pointShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
	pointShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(NodeVertex, totalDeformation), 1, sizeof(NodeVertex));

	pointShaderProgram->setUniformValue("valueRange.minValue", valueRange.minTotalDeformation);
	pointShaderProgram->setUniformValue("valueRange.maxValue", valueRange.maxTotalDeformation);
}

void OpenGLWindow::bindWireframeShaderProgram()
{
	wireframeShaderProgram->bind();
	wireframeShaderProgram->enableAttributeArray(0);
	wireframeShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
}

void OpenGLWindow::bindShadedShaderProgram()
{
	shadedShaderProgram->bind();
	shadedShaderProgram->enableAttributeArray(0);
	shadedShaderProgram->enableAttributeArray(1);
	shadedShaderProgram->enableAttributeArray(2);
	shadedShaderProgram->enableAttributeArray(3);
	shadedShaderProgram->enableAttributeArray(4);
	shadedShaderProgram->enableAttributeArray(5);
	shadedShaderProgram->enableAttributeArray(6);
	shadedShaderProgram->enableAttributeArray(7);
	shadedShaderProgram->enableAttributeArray(8);
	shadedShaderProgram->enableAttributeArray(9);

	shadedShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(NodeVertex, totalDeformation), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(2, GL_FLOAT, offsetof(NodeVertex, deformation), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(3, GL_FLOAT, offsetof(NodeVertex, normalElasticStrain), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(4, GL_FLOAT, offsetof(NodeVertex, shearElasticStrain), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(5, GL_FLOAT, offsetof(NodeVertex, maximumPrincipalStress), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(6, GL_FLOAT, offsetof(NodeVertex, middlePrincipalStress), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(7, GL_FLOAT, offsetof(NodeVertex, minimumPrincipalStress), 1, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(8, GL_FLOAT, offsetof(NodeVertex, normalStress), 3, sizeof(NodeVertex));
	shadedShaderProgram->setAttributeBuffer(9, GL_FLOAT, offsetof(NodeVertex, shearStress), 3, sizeof(NodeVertex));

	shadedShaderProgram->setUniformValue("valueRange.minTotalDeformation", valueRange.minTotalDeformation);
	shadedShaderProgram->setUniformValue("valueRange.maxTotalDeformation", valueRange.maxTotalDeformation);

	shadedShaderProgram->setUniformValue("valueRange.minDeformation", valueRange.minDeformation);
	shadedShaderProgram->setUniformValue("valueRange.maxDeformation", valueRange.maxDeformation);

	shadedShaderProgram->setUniformValue("valueRange.minNormalElasticStrain", valueRange.minNormalElasticStrain);
	shadedShaderProgram->setUniformValue("valueRange.maxNormalElasticStrain", valueRange.maxNormalElasticStrain);

	shadedShaderProgram->setUniformValue("valueRange.minShearElasticStrain", valueRange.minShearElasticStrain);
	shadedShaderProgram->setUniformValue("valueRange.maxShearElasticStrain", valueRange.maxShearElasticStrain);

	shadedShaderProgram->setUniformValue("valueRange.minMaximumPrincipalStress", valueRange.minMaximumPrincipalStress);
	shadedShaderProgram->setUniformValue("valueRange.maxMaximumPrincipalStress", valueRange.maxMaximumPrincipalStress);

	shadedShaderProgram->setUniformValue("valueRange.minMiddlePrincipalStress", valueRange.minMiddlePrincipalStress);
	shadedShaderProgram->setUniformValue("valueRange.maxMiddlePrincipalStress", valueRange.maxMiddlePrincipalStress);

	shadedShaderProgram->setUniformValue("valueRange.minMinimumPrincipalStress", valueRange.minMinimumPrincipalStress);
	shadedShaderProgram->setUniformValue("maxMinimumPrincipalStress", valueRange.maxMinimumPrincipalStress);

	shadedShaderProgram->setUniformValue("valueRange.minNormalStress", valueRange.minNormalStress);
	shadedShaderProgram->setUniformValue("valueRange.maxNormalStress", valueRange.maxNormalStress);

	shadedShaderProgram->setUniformValue("valueRange.minShearStress", valueRange.minShearStress);
	shadedShaderProgram->setUniformValue("valueRange.maxShearStress", valueRange.maxShearStress);
}

void OpenGLWindow::bindPickShaderProgram()
{
	pickShaderProgram->bind();
	pickShaderProgram->enableAttributeArray(0);
	pickShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(NodeVertex, position), 3, sizeof(NodeVertex));
}

void OpenGLWindow::initResources()
{
	// 创建基础节点顶点缓存对象
	nodeVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	nodeVBO.create();
	nodeVBO.bind();
	nodeVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	nodeVBO.allocate(nodeVertices.constData(), nodeVertices.count() * sizeof(NodeVertex));

	// 创建点模式相关渲染资源
	{
		// create the vertex array object
		pointVAO.create();
		pointVAO.bind();
		pointVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		pointVBO.create();
		pointVBO.bind();
		pointVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		pointVBO.allocate(uniformGrids.points.constData(), uniformGrids.points.count() * sizeof(NodeVertex));

		bindPointShaderProgram();
	}

	// 创建线框模式相关渲染资源
	{
		// create the vertex array object
		wireframeVAO.create();
		wireframeVAO.bind();
		nodeVBO.bind();

		// create the index buffer object
		wireframeIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		wireframeIBO.create();
		wireframeIBO.bind();
		wireframeIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		wireframeIBO.allocate(wireframeIndices.constData(), wireframeIndices.count() * sizeof(uint32_t));

		bindWireframeShaderProgram();
	}

	// 创建单元模式相关渲染资源
	{
		zoneVAO.create();
		zoneVAO.bind();
		nodeVBO.bind();

		// create the index buffer object
		zoneIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		zoneIBO.create();
		zoneIBO.bind();
		zoneIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		zoneIBO.allocate(zoneIndices.constData(), zoneIndices.count() * sizeof(uint32_t));
		bindShadedShaderProgram();
	}

	// 创建Face相关渲染资源
	{
		facetVAO.create();
		facetVAO.bind();
		nodeVBO.bind();

		// create the index buffer object
		facetIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		facetIBO.create();
		facetIBO.bind();
		facetIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		facetIBO.allocate(facetIndices.constData(), facetIndices.count() * sizeof(uint32_t));
		bindShadedShaderProgram();
	}

	// 创建截面相关渲染资源
	{
		const int kMaxSectionVertexNum = 5000;
		sectionVertices.resize(kMaxSectionVertexNum);
		sectionIndices.resize(sectionVertices.count() * 10);

		sectionVAO.create();
		sectionVAO.bind();

		sectionVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		sectionVBO.create();
		sectionVBO.bind();
		sectionVBO.setUsagePattern(QOpenGLBuffer::UsagePattern::DynamicDraw);
		sectionVBO.allocate(sectionVertices.constData(), sectionVertices.count() * sizeof(NodeVertex));

		// create the index buffer object
		sectionIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		sectionIBO.create();
		sectionIBO.bind();
		sectionIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
		sectionIBO.allocate(sectionIndices.constData(), sectionIndices.count() * sizeof(uint32_t));
		bindShadedShaderProgram();

		// 初始化截面线框资源
		sectionWireframeIndices.resize(sectionVertices.count() * 5);

		sectionWireframeVAO.create();
		sectionWireframeVAO.bind();
		sectionVBO.bind();

		// create the index buffer object
		sectionWireframeIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		sectionWireframeIBO.create();
		sectionWireframeIBO.bind();
		sectionWireframeIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
		sectionWireframeIBO.allocate(sectionWireframeIndices.constData(), sectionWireframeIndices.count() * sizeof(uint32_t));

		sectionVertices.clear();
		sectionIndices.clear();
		sectionWireframeIndices.clear();
		bindWireframeShaderProgram();
	}

	// 创建等值面相关渲染资源
	{
		const int kMaxIsosurfaceVertexNum = 30000;
		isosurfaceVertices.resize(kMaxIsosurfaceVertexNum);
		isosurfaceIndices.resize(isosurfaceVertices.count() * 10);

		isosurfaceVAO.create();
		isosurfaceVAO.bind();

		isosurfaceVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		isosurfaceVBO.create();
		isosurfaceVBO.bind();
		isosurfaceVBO.setUsagePattern(QOpenGLBuffer::UsagePattern::DynamicDraw);
		isosurfaceVBO.allocate(isosurfaceVertices.constData(), isosurfaceVertices.count() * sizeof(NodeVertex));

		// create the index buffer object
		isosurfaceIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		isosurfaceIBO.create();
		isosurfaceIBO.bind();
		isosurfaceIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
		isosurfaceIBO.allocate(isosurfaceIndices.constData(), isosurfaceIndices.count() * sizeof(uint32_t));

		isosurfaceVertices.clear();
		isosurfaceIndices.clear();
		bindPointShaderProgram();
	}

	// 创建等值线相关渲染资源
	{
		const int kMaxIsolineVertexNum = 50000;
		isolineVertices.resize(kMaxIsolineVertexNum);

		isolineVAO.create();
		isolineVAO.bind();

		isolineVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		isolineVBO.create();
		isolineVBO.bind();
		isolineVBO.setUsagePattern(QOpenGLBuffer::UsagePattern::DynamicDraw);
		isolineVBO.allocate(isolineVertices.constData(), isolineVertices.count() * sizeof(NodeVertex));

		isolineVertices.clear();
		bindPointShaderProgram();
	}

	// 创建选择模式相关渲染资源
	{
		const int kMaxPickIndexNum = 100;
		pickIndices.resize(kMaxPickIndexNum);
		//pickIndices = { 0, 1 };
		//pickVertices.resize(2);

		pickVAO.create();
		pickVAO.bind();
		nodeVBO.bind();

		//pickVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		//pickVBO.create();
		//pickVBO.bind();
		//pickVBO.setUsagePattern(QOpenGLBuffer::UsagePattern::DynamicDraw);
		//pickVBO.allocate(pickVertices.constData(), pickVertices.count() * sizeof(NodeVertex));

		// create the index buffer object
		pickIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		pickIBO.create();
		pickIBO.bind();
		pickIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
		pickIBO.allocate(pickIndices.constData(), pickIndices.count() * sizeof(uint32_t));
		bindPickShaderProgram();
	}

	// 创建obj模型相关渲染资源
	{
		objVAO.create();
		objVAO.bind();

		objVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		objVBO.create();
		objVBO.bind();
		objVBO.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
		objVBO.allocate(objMesh.vertices.constData(), objMesh.vertices.count() * sizeof(QVector3D));

		// create the index buffer object
		objIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
		objIBO.create();
		objIBO.bind();
		objIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
		objIBO.allocate(objIndices.constData(), objIndices.count() * sizeof(uint32_t));
	}
}

void OpenGLWindow::cleanResources()
{
	Zone::facetID = 0;
	nodeVertices.clear();
	exteriorFacets.clear();
	mesh.clear();
	objMesh.clear();
	zones.clear();
	valueRange.reset();
	zoneTypes.clear();
	GeoUtil::destroyBVHTree(zoneBVHRoot);
	GeoUtil::destroyBVHTree(faceBVHRoot);
	uniformGrids.clear();
	wireframeIndices.clear();
	zoneIndices.clear();
	facetIndices.clear();
	sectionVertices.clear();
	sectionIndices.clear();
	sectionWireframeIndices.clear();
	isosurfaceVertices.clear();
	isosurfaceIndices.clear();
	isolineVertices.clear();
	pickIndices.clear();
	objIndices.clear();

	makeCurrent();
	nodeVBO.destroy();
	wireframeVAO.destroy();
	wireframeIBO.destroy();
	zoneVAO.destroy();
	zoneIBO.destroy();
	facetVAO.destroy();
	facetIBO.destroy();
	sectionVAO.destroy();
	sectionVBO.destroy();
	sectionIBO.destroy();
	sectionWireframeVAO.destroy();
	sectionWireframeIBO.destroy();
	isolineVAO.destroy();
	isolineVBO.destroy();
	pickVAO.destroy();
	pickIBO.destroy();
	objVAO.destroy();
	objVBO.destroy();
	objIBO.destroy();
}
