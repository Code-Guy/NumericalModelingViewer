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

	QVector3D size();
	void scale(float s);
	Bound scaled(float s);
	void combine(const QVector3D& position);
	void combine(const Bound& bound);
	int maxDim();
	bool intersect(const Plane& plane);
	bool contain(const QVector3D& point) const;
	void cache();
	void reset();

	bool calcPointPlaneSide(const QVector3D& point, const Plane& plane, float epsilon = 0.001f);
};

struct Edge
{
	std::array<uint32_t, 2> vertices;
};

struct Face
{
	std::array<uint32_t, 3> vertices;
	std::array<Edge, 3> edges;
	Bound bound;
	bool visited = false;
};

struct Mesh
{
	QVector<QVector3D> vertices;
	QMap<Edge, QVector<uint32_t>> edges;
	QVector<Face> faces;
};

struct BVHTreeNode
{
	QVector<uint32_t> zones;

	Bound bound;
	BVHTreeNode* children[2] = { nullptr, nullptr };
	int depth = 0;
	bool isLeaf = false;
};

struct NodeVertex
{
	QVector3D position;
	float totalDeformation;
	QVector3D deformation;
	QVector3D normalElasticStrain;
	QVector3D shearElasticStrain;
	float maximumPrincipalStress;
	float middlePrincipalStress;
	float minimumPrincipalStress;
	QVector3D normalStress;
	QVector3D shearStress;
};

struct ValueRange
{
	float minTotalDeformation = kMaxVal;
	float maxTotalDeformation = kMinVal;

	QVector3D minDeformation = kMaxVec3;
	QVector3D maxDeformation = kMinVec3;

	QVector3D minNormalElasticStrain = kMaxVec3;
	QVector3D maxNormalElasticStrain = kMinVec3;

	QVector3D minShearElasticStrain = kMaxVec3;
	QVector3D maxShearElasticStrain = kMinVec3;

	float minMaximumPrincipalStress = kMaxVal;
	float maxMaximumPrincipalStress = kMinVal;

	float minMiddlePrincipalStress = kMaxVal;
	float maxMiddlePrincipalStress = kMinVal;

	float minMinimumPrincipalStress = kMaxVal;
	float maxMinimumPrincipalStress = kMinVal;

	QVector3D minNormalStress = kMaxVec3;
	QVector3D maxNormalStress = kMinVec3;

	QVector3D minShearStress = kMaxVec3;
	QVector3D maxShearStress = kMinVec3;
};

enum ZoneType
{
	Brick, Wedge, Pyramid, DegeneratedBrick, Tetrahedron
};

enum FacetType
{
	Q4, T3
};

struct Zone
{
	ZoneType type;
	int vertexNum;
	int edgeNum;
	quint32 vertices[8];
	quint32 edges[24];

	Bound bound;
	bool visited = false;

	float values[8];
	Bound valueBound;
	void cache(const QVector<NodeVertex>& nodeVertices);
	bool interp(const QVector3D& point, float& value) const;
};

struct Facet
{
	FacetType type;
	int num;
	quint32 indices[4];
};

struct UniformGrids
{
	int dim[3];
	Bound bound;
	std::vector<NodeVertex> values;
};

// Edge操作符重载和哈希函数
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

// Face操作符重载和哈希函数
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

// 基础向量操作函数
inline QVector3D qMinVec3(const QVector3D& lhs, const QVector3D& rhs)
{
	return QVector3D(
		qMin(lhs[0], rhs[0]),
		qMin(lhs[1], rhs[1]),
		qMin(lhs[2], rhs[2])
	);
}

inline QVector3D qMaxVec3(const QVector3D& lhs, const QVector3D& rhs)
{
	return QVector3D(
		qMax(lhs[0], rhs[0]),
		qMax(lhs[1], rhs[1]),
		qMax(lhs[2], rhs[2])
	);
}

template <typename T>
T qLerp(T a, T b, T t)
{
	return a * (1 - t) + b * t;
}

template <typename T>
T qBiLerp(T xa, T xb, T ya, T yb, T xt, T yt)
{
	float m = qLerp(xa, xb, xt);
	float n = qLerp(ya, yb, xt);
	return qLerp(m, n, yt);
}

template <typename T>
T qTriLerp(T zxa0, T zxb0, T zya0, T zyb0,
	T zxa1, T zxb1, T zya1, T zyb1,
	T xt, T yt, T zt)
{
	float m = qBiLerp(zxa0, zxb0, zya0, zyb0, xt, yt);
	float n = qBiLerp(zxa1, zxb1, zya1, zyb1, xt, yt);
	return qLerp(m, n, zt);
}