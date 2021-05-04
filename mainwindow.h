#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "openglwindow.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
	void onDisplayModeComboBoxCurrentIndexChanged(int index);

	void onPlaneOriginXLineEditTextChanged(const QString& text);
	void onPlaneOriginYLineEditTextChanged(const QString& text);
	void onPlaneOriginZLineEditTextChanged(const QString& text);
	void onPlaneNormalXLineEditTextChanged(const QString& text);
	void onPlaneNormalYLineEditTextChanged(const QString& text);
	void onPlaneNormalZLineEditTextChanged(const QString& text);
	void onDisableClipCheckBoxStateChanged(int state);

	void onIsosurfaceValueChanged(int value);
	void onIsosurfaceShowWireframeCheckBoxStateChanged(int state);

	void onIsolineValueChanged(int value);
	void onIsolineShowWireframeCheckBoxStateChanged(int state);

	void openFile();
	void exportToEDB();

	void onModelStartLoad();
	void onModelFinishLoad();

private:
	void setLayoutVisible(QLayout* layout, bool flag);

    Ui::MainWindow *ui;

	QVector<QLayout*> displayModeLayouts;
	Plane clipPlane;
	QVector2D isoValueRange;
};

#endif // MAINWINDOW_H
