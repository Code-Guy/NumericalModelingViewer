#pragma once

#include <QMatrix4x4>
#include <QVector>
#include <QSet>
#include <QMap>
#include <array>

const float kMaxVal = 1e8f;
const float kMinVal = -kMaxVal;
const int kInvalidIndex = -1;
const QVector3D kMaxVec3 = QVector3D(kMaxVal, kMaxVal, kMaxVal);
const QVector3D kMinVec3 = QVector3D(kMinVal, kMinVal, kMinVal);

struct Edge
{
	std::array<uint32_t, 2> vertices;
};

inline bool operator<(const Edge& lhs, const Edge& rhs)
{
	if (lhs.vertices[0] * lhs.vertices[1] < rhs.vertices[0] * rhs.vertices[1])
	{
		return true;
	}

	if (lhs.vertices[0] * lhs.vertices[1] == rhs.vertices[0] * rhs.vertices[1] && 
		lhs.vertices[0] + lhs.vertices[1] < rhs.vertices[0] + rhs.vertices[1])
	{
		return true;
	}

	return false;
}

inline bool operator==(const Edge& lhs, const Edge& rhs)
{
	return (lhs.vertices[0] == rhs.vertices[0] && lhs.vertices[1] == rhs.vertices[1]) ||
		(lhs.vertices[0] == rhs.vertices[1] && lhs.vertices[1] == rhs.vertices[0]);
}

struct Face
{
	std::array<uint32_t, 3> vertices;
	std::array<Edge, 3> edges;
	bool visited = false;
};

struct Plane
{
	QVector3D origin;
	QVector3D normal;
	float d;
};

struct ClipLine
{
	QVector<QVector3D> vertices;
};

struct Mesh
{
	QVector<QVector3D> vertices;
	QMap<Edge, QVector<uint32_t>> edges;
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
	static void fixWindingOrder(Mesh& mesh);
	static QVector<ClipLine> clipMesh(Mesh& mesh, const Plane& plane);

private:
	static void fixWindingOrder(Mesh& mesh, const Face& mainFace, Face& neighborFace);
	static void flipWindingOrder(Face& face);
	static bool clipEdge(const Mesh& mesh, const Edge& edge, const Plane& plane, QVector3D& intersection);
};