#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    displayModeLayouts.append(ui->clipZoneHorizontalLayout);
    displayModeLayouts.append(ui->isosurfaceHorizontalLayout);
    displayModeLayouts.append(ui->isolineHorizontalLayout);
    onDisplayModeComboBoxCurrentIndexChanged(ui->displayModeComboBox->currentIndex());

    connect(ui->openGLWidget, SIGNAL(onModelStartLoad()), this, SLOT(onModelStartLoad()));
    connect(ui->openGLWidget, SIGNAL(onModelFinishLoad()), this, SLOT(onModelFinishLoad()));

    connect(ui->openAction, SIGNAL(triggered()), this, SLOT(openFile()));
    connect(ui->exportAction, SIGNAL(triggered()), this, SLOT(exportToEDB()));

    connect(ui->displayModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onDisplayModeComboBoxCurrentIndexChanged(int)));
    connect(ui->pickModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onPickModeComboBoxCurrentIndexChanged(int)));

    connect(ui->planeOriginXLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onPlaneOriginXLineEditTextChanged(const QString&)));
    connect(ui->planeOriginYLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onPlaneOriginYLineEditTextChanged(const QString&)));
    connect(ui->planeOriginZLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onPlaneOriginZLineEditTextChanged(const QString&)));
	connect(ui->planeNormalXLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onPlaneNormalXLineEditTextChanged(const QString&)));
	connect(ui->planeNormalYLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onPlaneNormalYLineEditTextChanged(const QString&)));
	connect(ui->planeNormalZLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onPlaneNormalZLineEditTextChanged(const QString&)));
	connect(ui->disableClipCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onDisableClipCheckBoxStateChanged(int)));

    connect(ui->isosurfaceValueSlider, SIGNAL(valueChanged(int)), this, SLOT(onIsosurfaceValueChanged(int)));
    connect(ui->isosurfaceShowWireframeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onIsosurfaceShowWireframeCheckBoxStateChanged(int)));

    connect(ui->isolineValueSlider, SIGNAL(valueChanged(int)), this, SLOT(onIsolineValueChanged(int)));
	connect(ui->isolineShowWireframeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onIsolineShowWireframeCheckBoxStateChanged(int)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onDisplayModeComboBoxCurrentIndexChanged(int index)
{
    for (QLayout* displayModeLayout : displayModeLayouts)
    {
        setLayoutVisible(displayModeLayout, false);
    }
    setLayoutVisible(displayModeLayouts[index], true);

    ui->openGLWidget->setDisplayMode((DisplayMode)index);
}

void MainWindow::onPickModeComboBoxCurrentIndexChanged(int index)
{
    ui->openGLWidget->setPickMode((PickMode)index);
}

void MainWindow::onPlaneOriginXLineEditTextChanged(const QString& text)
{
    clipPlane.origin[0] = text.toFloat();
    ui->openGLWidget->setClipPlane(clipPlane);
}

void MainWindow::onPlaneOriginYLineEditTextChanged(const QString& text)
{
    clipPlane.origin[1] = text.toFloat();
	ui->openGLWidget->setClipPlane(clipPlane);
}

void MainWindow::onPlaneOriginZLineEditTextChanged(const QString& text)
{
    clipPlane.origin[2] = text.toFloat();
	ui->openGLWidget->setClipPlane(clipPlane);
}

void MainWindow::onPlaneNormalXLineEditTextChanged(const QString& text)
{
    clipPlane.normal[0] = text.toFloat();
	ui->openGLWidget->setClipPlane(clipPlane);
}

void MainWindow::onPlaneNormalYLineEditTextChanged(const QString& text)
{
    clipPlane.normal[1] = text.toFloat();
    ui->openGLWidget->setClipPlane(clipPlane);
}

void MainWindow::onPlaneNormalZLineEditTextChanged(const QString& text)
{
    clipPlane.normal[2] = text.toFloat();
	ui->openGLWidget->setClipPlane(clipPlane);
}

void MainWindow::onDisableClipCheckBoxStateChanged(int state)
{
    ui->openGLWidget->setDisableClip(state == Qt::Checked);
}

void MainWindow::onIsosurfaceValueChanged(int value)
{
    ui->openGLWidget->setIsosurfaceValue(qMapClampRange((float)value, 0.0f, 99.0f, isoValueRange[0], isoValueRange[1]));
}

void MainWindow::onIsosurfaceShowWireframeCheckBoxStateChanged(int state)
{
    ui->openGLWidget->setShowIsosurfaceWireframe(state == Qt::Checked);
}

void MainWindow::onIsolineValueChanged(int value)
{
    ui->openGLWidget->setIsolineValue(qMapClampRange((float)value, 0.0f, 99.0f, isoValueRange[0], isoValueRange[1]));
}

void MainWindow::onIsolineShowWireframeCheckBoxStateChanged(int state)
{
	ui->openGLWidget->setShowIsolineWireframe(state == Qt::Checked);
}

void MainWindow::openFile()
{
	QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("打开"), "asset/data/", tr("Database file(*.edb);;f3grid file(*.f3grid)"));
	if (!fileName.isEmpty())
	{
		ui->openGLWidget->openFile(fileName);
	}
}

void MainWindow::exportToEDB()
{
	QString exportPath = QFileDialog::getSaveFileName(this, QStringLiteral("导出"), "asset/data/export", tr("Database file(*.edb)"));
	if (!exportPath.isEmpty())
	{
		ui->openGLWidget->exportToEDB(exportPath);
	}
}

void MainWindow::onModelStartLoad()
{

}

void MainWindow::onModelFinishLoad()
{
	clipPlane.origin = QVector3D(0.0f, 0.0f, 100.0f);
	clipPlane.normal = QVector3D(0.0f, -2.0f, -1.0f);
	isoValueRange = ui->openGLWidget->getIsoValueRange();

	// 初始化界面数值
	ui->planeOriginXLineEdit->setText(QString::number(clipPlane.origin.x()));
	ui->planeOriginYLineEdit->setText(QString::number(clipPlane.origin.y()));
	ui->planeOriginZLineEdit->setText(QString::number(clipPlane.origin.z()));
	ui->planeNormalXLineEdit->setText(QString::number(clipPlane.normal.x()));
	ui->planeNormalYLineEdit->setText(QString::number(clipPlane.normal.y()));
	ui->planeNormalZLineEdit->setText(QString::number(clipPlane.normal.z()));

	ui->isosurfaceValueSlider->setValue(70);
	ui->isolineValueSlider->setValue(ui->isosurfaceValueSlider->value());
}

void MainWindow::setLayoutVisible(QLayout* layout, bool flag)
{
	for (int i = 0; i < layout->count(); ++i)
    {
		QWidget* widget = layout->itemAt(i)->widget();
        if (widget)
        {
            widget->setVisible(flag);
        }
        else
        {
            ((QSpacerItem*)layout->itemAt(i))->changeSize(flag ? 15 : 0, flag ? 18 : 0, QSizePolicy::Fixed);
        }
	}
}
