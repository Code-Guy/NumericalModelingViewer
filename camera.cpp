#include "camera.h"
#include <QtMath>
#include <QDebug>

Camera::Camera(QVector3D position,
	float yaw, float pitch, 
	float speed, float sensitivity) :
	position(position),
	yaw(yaw), pitch(pitch),
	speed(speed), sensitivity(sensitivity)
{
	moveForward = false;
	moveBack = false;
	moveLeft = false;
	moveRight = false;

	mouseLRButtonPressed = false;

	updatePose();
}

void Camera::setFovy(float fovy)
{
	this->fovy = fovy;
}

void Camera::setAspect(float aspect)
{
	this->aspect = aspect;
}

void Camera::setClipping(float zNear, float zFar)
{
	this->zNear = zNear;
	this->zFar = zFar;
}

void Camera::tick(float deltaTime)
{
	float offset = speed * deltaTime;

	if (moveForward)
	{
		position += forward * offset;
	}
	if (moveBack)
	{
		position -= forward * offset;
	}
	if (moveLeft)
	{
		position -= right * offset;
	}
	if (moveRight)
	{
		position += right * offset;
	}
}

QVector3D Camera::getPosition()
{
	return position;
}

QMatrix4x4 Camera::getViewMatrix()
{
	QMatrix4x4 viewMat;
	viewMat.lookAt(position, position + forward, up);
	return viewMat;
}

QMatrix4x4 Camera::getPerspectiveMatrix()
{
	QMatrix4x4 projMat;
	projMat.perspective(fovy, aspect, zNear, zFar);
	return projMat;
}

QMatrix4x4 Camera::getViewPerspectiveMatrix()
{
	return getPerspectiveMatrix() * getViewMatrix();
}

void Camera::onKeyPressed(int key)
{
	if (key == Qt::Key_W)
	{
		moveForward = true;
	}
	if (key == Qt::Key_S)
	{
		moveBack = true;
	}
	if (key == Qt::Key_A)
	{
		moveLeft = true;
	}
	if (key == Qt::Key_D)
	{
		moveRight = true;
	}
}

void Camera::onKeyReleased(int key)
{
	if (key == Qt::Key_W)
	{
		moveForward = false;
	}
	if (key == Qt::Key_S)
	{
		moveBack = false;
	}
	if (key == Qt::Key_A)
	{
		moveLeft = false;
	}
	if (key == Qt::Key_D)
	{
		moveRight = false;
	}
}

void Camera::onMouseMoved(float x, float y)
{
	if (!mouseLRButtonPressed)
	{
		return;
	}

	float dx = (x - lastMouseX) * sensitivity;
	float dy = (y - lastMouseY) * sensitivity;
	lastMouseX = x;
	lastMouseY = y;

	yaw += dx;
	pitch -= dy;

	if (yaw < 0.0f)
	{
		yaw += 360.0f;
	}
	else if (yaw > 360.0f)
	{
		yaw -= 360.0f;
	}
	pitch = qBound(-89.0f, pitch, 89.0f);

	updatePose();
}

void Camera::onMousePressed(int button, float x, float y)
{
	if (button == Qt::LeftButton || button == Qt::RightButton)
	{
		mouseLRButtonPressed = true;
		lastMouseX = x;
		lastMouseY = y;
	}
}

void Camera::onMouseReleased(int button)
{
	if (button == Qt::LeftButton || button == Qt::RightButton)
	{
		mouseLRButtonPressed = false;
	}
}

void Camera::updatePose()
{
	forward.setX(qCos(qDegreesToRadians(yaw)) * qCos(qDegreesToRadians(pitch)));
	forward.setY(qSin(qDegreesToRadians(pitch)));
	forward.setZ(qSin(qDegreesToRadians(yaw)) * qCos(qDegreesToRadians(pitch)));
	forward.normalize();

	right = QVector3D::crossProduct(forward, QVector3D(0.0f, 1.0f, 0.0f)).normalized();
	up = QVector3D::crossProduct(right, forward).normalized();
}
