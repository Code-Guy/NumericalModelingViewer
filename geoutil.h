#pragma once

#include "geotypes.h"

// 几何计算实用类
class GeoUtil
{
public:
	static void loadObjMesh(const char* fileName, Mesh& mesh);
	static void addFace(Mesh& mesh, uint32_t v0, uint32_t v1, uint32_t v2);
	static void cleanMesh(Mesh& mesh);
	static void fixWindingOrder(Mesh& mesh);
	static void clipZones(QVector<Zone>& zones, const Plane& plane, BVHTreeNode* root, QVector<NodeVertex>& nodeVertices, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QVector<uint32_t>& sectionWireframeIndices);
	static QVector<ClipLine> genIsolines(Mesh& mesh, QVector<NodeVertex>& nodeVertices, float value, BVHTreeNode* root);
	static bool validateMesh(Mesh& mesh);
	static bool interpZones(const QVector<Zone>& zones, BVHTreeNode* node, const QVector3D& point, float& value);
	static bool inZones(const QVector<Zone>& zones, BVHTreeNode* node, const QVector3D& point);

	static BVHTreeNode* buildBVHTree(const QVector<Zone>& zones);
	static BVHTreeNode* buildBVHTree(const Mesh& mesh);

private:
	static void fixWindingOrder(Mesh& mesh, const Face& mainFace, Face& neighborFace);
	static void flipWindingOrder(Face& face);
	static bool clipZone(const Zone& zone, const Plane& plane, QVector<NodeVertex>& nodeVertices, QMap<Edge, uint32_t>& intersectionIndexMap, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QSet<Edge>& sectionWireframes);
	static bool isManifordFace(const Mesh& mesh, const Face& face, bool strict = true);
	static void traverseMesh(Mesh& mesh);
	static void resetMeshVisited(Mesh& mesh);
	static void resetZoneVisited(QVector<Zone>& zones);
	static void resetBVHTree(BVHTreeNode* node);
	static void clipZones(QVector<Zone>& zones, const Plane& plane, BVHTreeNode* node, QVector<NodeVertex>& nodeVertices, QMap<Edge, uint32_t>& intersectionIndexMap, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QSet<Edge>& sectionWireframes);
	static void findAllIsoEdges(Mesh& mesh, QVector<NodeVertex>& nodeVertices, float value, BVHTreeNode* node, QMap<Edge, QVector3D>& hits);
	static void findIsoEdges(const Mesh& mesh, QVector<NodeVertex>& nodeVertices, const Face& face, float value, QMap<Edge, QVector3D>& hits);

	static BVHTreeNode* buildBVHTree(const QVector<Zone>& zones, QVector<uint32_t>& zoneIndices, int begin, int end);
	static BVHTreeNode* buildBVHTree(const Mesh& mesh, QVector<uint32_t>& faces, int begin, int end);
};