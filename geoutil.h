#pragma once

#include <QMatrix4x4>
#include <QVector>

const float kMaxVal = 1e8f;
const float kMinVal = -kMaxVal;
const QVector3D kMaxVec3 = QVector3D(kMaxVal, kMaxVal, kMaxVal);
const QVector3D kMinVec3 = QVector3D(kMinVal, kMinVal, kMinVal);

struct Vertex
{
	QVector3D position;
};

struct Face
{
	uint32_t indices[3];
};

struct Plane
{
	QVector3D origin;
	QVector3D normal;
};

struct ClipLine
{
	QVector<Vertex> vertices;
};

struct Mesh
{
	QVector<Vertex> vertices;
	QVector<Face> faces;
};

struct BoundingBox
{
	QVector3D min = kMaxVec3;
	QVector3D max = kMinVec3;

	void scale(float s)
	{
		QVector3D offset = (max - min) * (s - 1.0f);
		min -= offset;
		max += offset;
	}
};

class GeoUtil
{
public:
	static QVector<ClipLine> clipMesh(const Mesh& mesh, const Plane& plane);

private:
};