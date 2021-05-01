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
	bool degenerated = false;

	Plane() {}
	Plane(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2);
	bool checkSide(const QVector3D& point) const;
};

struct Bound
{
	QVector3D min = kMaxVec3;
	QVector3D max = kMinVec3;
	QVector3D centriod;
	QVector3D corners[8];
	int intersectFlag;

	QVector3D size() const;
	void scale(float s);
	Bound scaled(float s) const;
	void combine(const QVector3D& position);
	void combine(const Bound& bound);
	int maxDim();
	bool intersect(const Plane& plane);
	bool contain(const QVector3D& point) const;
	void cache();
	void reset();
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

struct ClipLine
{
	QList<QVector3D> vertices;
};

struct BVHTreeNode
{
	QVector<uint32_t> zones;
	QVector<uint32_t> faces;

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
	QMatrix4x4 invertedBasisMatrix;
	QVector<Plane> planes;
	QVector3D origin;
	bool isValid() const;
	void cache(const QVector<NodeVertex>& nodeVertices);
	bool contain(const QVector3D& point) const;
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
	std::array<int, 3> dim;
	Bound bound;
	QVector<NodeVertex> points;
	QVector<float> voxelData;
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

inline float qMax3(float a, float b, float c)
{
	if (a > b && a > c)
	{
		return a;
	}
	if (b > a && b > c)
	{
		return b;
	}
	return c;
}

inline float qMin3(float a, float b, float c)
{
	if (a < b && a < c)
	{
		return a;
	}
	if (b < a && b < c)
	{
		return b;
	}
	return c;
}

template <typename T>
T qLerp(const T& a, const T& b, float t)
{
	return a * (1 - t) + b * t;
}

template <typename T>
T qBiLerp(const T& xa, const T& xb, const T& ya, const T& yb, float xt, float yt)
{
	float m = qLerp(xa, xb, xt);
	float n = qLerp(ya, yb, xt);
	return qLerp(m, n, yt);
}

template <typename T>
T qTriLerp(const T& zxa0, const T& zxb0, const T& zya0, const T& zyb0,
	const T& zxa1, const T& zxb1, const T& zya1, const T& zyb1,
	float xt, float yt, float zt)
{
	float m = qBiLerp(zxa0, zxb0, zya0, zyb0, xt, yt);
	float n = qBiLerp(zxa1, zxb1, zya1, zyb1, xt, yt);
	return qLerp(m, n, zt);
}

template <typename T>
T qClamp(const T& value, const T& min, const T& max)
{
	if (value < min)
	{
		return min;
	}
	if (value > max)
	{
		return max;
	}
	return value;
}

template <typename T>
T qMapClampRange(const T& value, const T& fromMin, const T& fromMax, const T& toMin, const T& toMax)
{
	return toMin + (toMax - toMin) * (value - fromMin) / (fromMax - fromMin);
}

int qMaxDim(const QVector3D& v);
float qMaxDimVal(const QVector3D& v);
bool qIsNearlyZero(const QVector3D& v0, float epsilon = 0.001f);
bool qIsNearlyEqual(const QVector3D& v0, const QVector3D& v1, float epsilon = 0.001f);
float qManhattaDistance(const QVector3D& v0, const QVector3D& v1);
QVector3D qToVec3(const std::array<double, 3>& arr3);
std::array<double, 3> qToArr3(const QVector3D& vec3);