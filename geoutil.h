#pragma once

#include <QMatrix4x4>
#include <QVector>
#include <QList>
#include <QSet>
#include <QMap>
#include <array>

const float kMaxVal = 1e8f;
const float kMinVal = -kMaxVal;
const uint32_t kInvalidIndex = 1e8;
const QVector3D kMaxVec3 = QVector3D(kMaxVal, kMaxVal, kMaxVal);
const QVector3D kMinVec3 = QVector3D(kMinVal, kMinVal, kMinVal);

struct Plane
{
	QVector3D origin;
	QVector3D normal;
	float dist;
};

struct Bound
{
	QVector3D min = kMaxVec3;
	QVector3D max = kMinVec3;
	QVector3D centriod;
	QVector3D corners[8];
	int intersectFlag;

	void scale(float s);
	void combine(const QVector3D& position);
	void combine(const Bound& bound);
	int maxDim();
	bool intersect(const Plane& plane);
	bool calcPointPlaneSide(const QVector3D& point, const Plane& plane, float epsilon = 0.001f);

	void cache();
	void reset();
};

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
	Bound bound;
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

struct ClipLine
{
	QList<QVector3D> vertices;
};

struct Mesh
{
	QVector<QVector3D> vertices;
	QMap<Edge, QVector<uint32_t>> edges;
	QVector<Face> faces;
};

struct BVHTreeNode
{
	QVector<uint32_t> faces;
	int num;
	Bound bound;

	BVHTreeNode* children[2];
	bool isLeaf;
};

QVector3D qMinVec3(const QVector3D& lhs, const QVector3D& rhs);
QVector3D qMaxVec3(const QVector3D& lhs, const QVector3D& rhs);

template <typename T>
T qLerp(T a, T b, T t)
{
	return a * (1 - t) + b * t;
}

class GeoUtil
{
public:
	static void loadObjMesh(const char* fileName, Mesh& mesh);
	static void addFace(Mesh& mesh, uint32_t v0, uint32_t v1, uint32_t v2);
	static void cleanMesh(Mesh& mesh);
	static void fixWindingOrder(Mesh& mesh);
	static QVector<ClipLine> clipMesh(Mesh& mesh, const Plane& plane, BVHTreeNode* root);
	static bool validateMesh(Mesh& mesh);
	static BVHTreeNode* buildBVHTree(const Mesh& mesh);

private:
	static void fixWindingOrder(Mesh& mesh, const Face& mainFace, Face& neighborFace);
	static void flipWindingOrder(Face& face);
	static void clipFace(const Mesh& mesh, const Face& face, const Plane& plane, QMap<Edge, QVector3D>& hits);
	static bool clipEdge(const Mesh& mesh, const Edge& edge, const Plane& plane, QVector3D& intersection);
	static bool isManifordFace(const Mesh& mesh, const Face& face, bool strict = true);
	static bool isBoundaryEdge(const Mesh& mesh, const Edge& edge);
	static void traverseMesh(Mesh& mesh);
	static void resetMeshVisited(Mesh& mesh);
	static BVHTreeNode* buildBVHTree(const Mesh& mesh, QVector<uint32_t>& faces, int begin, int end);
	static void resetBVHTree(BVHTreeNode* node);
	static void findAllClipEdges(Mesh& mesh, const Plane& plane, BVHTreeNode* node, QMap<Edge, QVector3D>& hits);
	static bool isVec3NearlyEqual(const QVector3D& lhs, const QVector3D& rhs, float epsilon = 0.001f);
};