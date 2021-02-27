#pragma once

#include <QtCore>
#include <QMatrix4x4>

class Camera
{
public:
	Camera(QVector3D position, 
		float yaw, float pitch,
		float speed, float sensitivity);

	void setFovy(float fovy);
	void setAspect(float aspect);
	void setClipping(float zNear, float zFar);

	void tick(float deltaTime);

	QVector3D getPosition();
	QMatrix4x4 getViewMatrix();
	QMatrix4x4 getPerspectiveMatrix();
	QMatrix4x4 getViewPerspectiveMatrix();

	void onKeyPressed(int key);
	void onKeyReleased(int key);
	void onMouseMoved(float x, float y);
	void onMousePressed(int button, float x, float y);
	void onMouseReleased(int button);

private:
	void updatePose();

	QVector3D position;
	float yaw;
	float pitch;

	float speed;
	float sensitivity;

	float fovy;
	float aspect;
	float zNear;
	float zFar;

	QVector3D forward;
	QVector3D right;
	QVector3D up;

	bool moveForward;
	bool moveBack;
	bool moveLeft;
	bool moveRight;

	bool mouseLRButtonPressed;
	float lastMouseX, lastMouseY;
};