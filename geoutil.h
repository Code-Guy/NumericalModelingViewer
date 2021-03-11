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

inline uint qHash(const Edge& edge, uint seed = 0)
{
	return qHash(edge.vertices[0] * edge.vertices[1]);
}

struct Face
{
	std::array<uint32_t, 3> vertices;
	std::array<Edge, 3> edges;
	bool visited = false;
};

inline bool operator==(const Face& lhs, const Face& rhs)
{
	for (int i = 0; i < 3; ++i)
	{
		bool foundEqual = false;
		for (int j = 0; j < 3; ++j)
		{
			if (rhs.vertices[j] == lhs.vertices[i])
			{
				foundEqual = true;
				break;
			}
		}

		if (!foundEqual)
		{
			return false;
		}
	}
	return true;
}

inline uint qHash(const Face& face, uint seed = 0)
{
	return qHash(face.vertices[0] * face.vertices[1] * face.vertices[2]);
}

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
	static void cleanMesh(Mesh& mesh);
	static void fixWindingOrder(Mesh& mesh);
	static QVector<ClipLine> clipMesh(Mesh& mesh, const Plane& plane);
	static bool checkValidMesh(Mesh& mesh);

private:
	static void fixWindingOrder(Mesh& mesh, const Face& mainFace, Face& neighborFace);
	static void flipWindingOrder(Face& face);
	static bool clipEdge(const Mesh& mesh, const Edge& edge, const Plane& plane, QVector3D& intersection);
	static bool checkManifordFace(const Mesh& mesh, const Face& face, bool strict = true);
	static void traverseMesh(Mesh& mesh);
	static void resetMeshVisited(Mesh& mesh);
};