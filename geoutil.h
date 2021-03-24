#pragma once

#include "geotypes.h"

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
T qTriLerp(T xza0, T xzb0, T yza0, T yzb0, 
	T xza1, T xzb1, T yza1, T yzb1, 
	T xt, T yt, T zt)
{
	float m = qBiLerp(xza0, xzb0, yza0, yzb0, xt, yt);
	float n = qBiLerp(xza1, xzb1, yza1, yzb1, xt, yt);
	return qLerp(m, n, zt);
}

// 几何计算实用类
class GeoUtil
{
public:
	static void loadObjMesh(const char* fileName, Mesh& mesh);
	static void addFace(Mesh& mesh, uint32_t v0, uint32_t v1, uint32_t v2);
	static void cleanMesh(Mesh& mesh);
	static void fixWindingOrder(Mesh& mesh);
	static void clipZones(QVector<Zone>& zones, const Plane& plane, BVHTreeNode* root, QVector<NodeVertex>& nodeVertices, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QVector<uint32_t>& sectionWireframeIndices);
	static bool validateMesh(Mesh& mesh);
	static BVHTreeNode* buildBVHTree(const QVector<Zone>& zones);
	static void interpUniformGrids(BVHTreeNode* root, UniformGrids& uniformGrids);

private:
	static void fixWindingOrder(Mesh& mesh, const Face& mainFace, Face& neighborFace);
	static void flipWindingOrder(Face& face);
	static bool clipZone(const Zone& zone, const Plane& plane, QVector<NodeVertex>& nodeVertices, QMap<Edge, uint32_t>& intersectionIndexMap, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QSet<Edge>& sectionWireframes);
	static bool isManifordFace(const Mesh& mesh, const Face& face, bool strict = true);
	static void traverseMesh(Mesh& mesh);
	static void resetMeshVisited(Mesh& mesh);
	static void resetZoneVisited(QVector<Zone>& zones);
	static BVHTreeNode* buildBVHTree(const QVector<Zone>& zones, QVector<uint32_t>& zoneIndices, int begin, int end);
	static void resetBVHTree(BVHTreeNode* node);
	static void clipZones(QVector<Zone>& zones, const Plane& plane, BVHTreeNode* node, QVector<NodeVertex>& nodeVertices, QMap<Edge, uint32_t>& intersectionIndexMap, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QSet<Edge>& sectionWireframes);
	static bool interpUniformGrid(const QVector<Zone>& zones, QVector<NodeVertex>& nodeVertices, BVHTreeNode* node, const QVector3D& point, float& value);
};