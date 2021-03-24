#include "geoutil.h"
#include <QQueue>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QtAlgorithms>
#include <algorithm>
#include <fstream>

void Bound::scale(float s)
{
	QVector3D offset = (max - min) * (s - 1.0f);
	min -= offset;
	max += offset;
}

Bound Bound::scaled(float s)
{
	QVector3D offset = (max - min) * (s - 1.0f);
	return Bound{ min - offset, max + offset };
}

void Bound::combine(const QVector3D& position)
{
	min = qMinVec3(min, position);
	max = qMaxVec3(max, position);
}

void Bound::combine(const Bound& bound)
{
	min = qMinVec3(min, bound.min);
	max = qMaxVec3(max, bound.max);
}

int Bound::maxDim()
{
	QVector3D size = max - min;
	if (size[0] > size[1] && size[0] > size[2])
	{
		return 0;
	}
	if (size[1] > size[0] && size[1] > size[2])
	{
		return 1;
	}
	return 2;
}

bool Bound::intersect(const Plane& plane)
{
	if (intersectFlag != -1)
	{
		return intersectFlag;
	}

	bool first = calcPointPlaneSide(corners[0], plane);
	for (int i = 1; i < 8; i++)
	{
		bool other = calcPointPlaneSide(corners[i], plane);
		if (other != first)
		{
			intersectFlag = 1;
			return true;
		}
	}

	intersectFlag = 0;
	return false;
}

bool Bound::calcPointPlaneSide(const QVector3D& point, const Plane& plane, float epsilon)
{
	return QVector3D::dotProduct(point, plane.normal) > plane.dist;
}

void Bound::cache()
{
	corners[0] = QVector3D(min[0], min[1], min[2]);
	corners[1] = QVector3D(min[0], min[1], max[2]);
	corners[2] = QVector3D(min[0], max[1], min[2]);
	corners[3] = QVector3D(min[0], max[1], max[2]);
	corners[4] = QVector3D(max[0], min[1], min[2]);
	corners[5] = QVector3D(max[0], min[1], max[2]);
	corners[6] = QVector3D(max[0], max[1], min[2]);
	corners[7] = QVector3D(max[0], max[1], max[2]);
}

void Bound::reset()
{
	intersectFlag = -1;
}

QVector3D qMinVec3(const QVector3D& lhs, const QVector3D& rhs)
{
	return QVector3D(
		qMin(lhs[0], rhs[0]),
		qMin(lhs[1], rhs[1]),
		qMin(lhs[2], rhs[2])
	);
}

QVector3D qMaxVec3(const QVector3D& lhs, const QVector3D& rhs)
{
	return QVector3D(
		qMax(lhs[0], rhs[0]),
		qMax(lhs[1], rhs[1]),
		qMax(lhs[2], rhs[2])
	);
}

void GeoUtil::loadObjMesh(const char* fileName, Mesh& mesh)
{
	QFile inputFile(fileName);
	if (inputFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&inputFile);
		while (!in.atEnd())
		{
			QChar header;
			in >> header;
			if (header == "v")
			{
				QVector3D vertex;
				in >> vertex[0] >> vertex[1] >> vertex[2];
				mesh.vertices.append(vertex);
			}
			else if (header == "f")
			{
				uint32_t v0, v1, v2;
				in >> v0 >> v1 >> v2;
				addFace(mesh, --v0, --v1, --v2);
			}
			in.readLine();
		}
		inputFile.close();
	}

	//std::ifstream ifs(fileName);
	//char line[1024];
	//while (!ifs.eof())
	//{
	//	std::string header;
	//	ifs >> header;

	//	if (header == "v")
	//	{
	//		QVector3D vertex;
	//		ifs >> vertex[0] >> vertex[1] >> vertex[2];
	//		mesh.vertices.append(vertex);
	//	}
	//	else if (header == "f")
	//	{
	//		uint32_t v0, v1, v2;
	//		ifs >> v0 >> v1 >> v2;
	//		addFace(mesh, --v0, --v1, --v2);
	//	}
	//	ifs.getline(line, 1024);
	//}
}

void GeoUtil::addFace(Mesh& mesh, uint32_t v0, uint32_t v1, uint32_t v2)
{
	uint32_t faceIndex = mesh.faces.count();
	mesh.faces.append(Face{ v0, v1, v2 });
	Face& face = mesh.faces.back();

	Edge edges[3] = { { v0, v1 }, { v1, v2 }, { v0, v2 } };
	for (int j = 0; j < 3; ++j)
	{
		Edge& edge = edges[j];
		auto iter = mesh.edges.find(edge);
		if (iter == mesh.edges.end())
		{
			mesh.edges.insert(edge, { faceIndex });
		}
		else
		{
			edge = iter.key();
			iter.value().append(faceIndex);
		}
		face.edges[j] = edge;
	}

	for (uint32_t v : face.vertices)
	{
		face.bound.combine(mesh.vertices[v]);
	}
	face.bound.centriod = (face.bound.min + face.bound.max) * 0.5f;
}

void GeoUtil::cleanMesh(Mesh& mesh)
{
	traverseMesh(mesh);

	// 删除不合法（重复 || 孤立 || 非流形）面
	QSet<Face> uniqueFaces;
	QVector<Face> invalidFaces;
	QVector<Face> validFaces;
	QMap<Edge, QVector<uint32_t>> validEdges;
	QMap<Edge, QVector<uint32_t>> invalidEdges;
	int validFaceIndex = 0;
	int invalidFaceIndex = 0;

	std::array<int, 3> invalidFaceCounter = { 0, 0, 0 };
	for (int i = 0; i < mesh.faces.count(); ++i)
	{
		Face& face = mesh.faces[i];
		bool existed = uniqueFaces.contains(face);
		if (!existed)
		{
			uniqueFaces.insert(face);
		}

		// 统计各种类型非法面的个数
		if (existed)
		{
			invalidFaceCounter[0]++;
		}
		if (!face.visited)
		{
			invalidFaceCounter[1]++;
		}
		if (!isManifordFace(mesh, face))
		{
			invalidFaceCounter[2]++;
		}

		if (existed || !face.visited || !isManifordFace(mesh, face))
		{
			for (Edge& edge : face.edges)
			{
				invalidEdges[edge].append(invalidFaceIndex);
			}

			invalidFaces.append(face);
			invalidFaceIndex++;
		}
		else
		{
			for (Edge& edge : face.edges)
			{
				validEdges[edge].append(validFaceIndex);
			}

			validFaces.append(face);
			validFaceIndex++;
		}
	}

	mesh.faces = validFaces;
	mesh.edges = validEdges;
	//mesh.faces = invalidFaces;
	//mesh.edges = invalidEdges;
}

void GeoUtil::fixWindingOrder(Mesh& mesh)
{
	// 重置被访问属性
	resetMeshVisited(mesh);

	QQueue<uint32_t> queue;
	queue.enqueue(0);
	mesh.faces[0].visited = true;
	
	while (!queue.isEmpty())
	{
		const Face& mainFace = mesh.faces[queue.dequeue()];
		for (const Edge& edge : mainFace.edges)
		{
			for (uint32_t f : mesh.edges[edge])
			{
				Face& neighborFace = mesh.faces[f];
				if (!neighborFace.visited)
				{
					neighborFace.visited = true;
					fixWindingOrder(mesh, mainFace, neighborFace);
					queue.enqueue(f);
				}
			}
		}
	}
}

void GeoUtil::clipZones(QVector<Zone>& zones, const Plane& plane, BVHTreeNode* root, QVector<NodeVertex>& nodeVertices, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QVector<uint32_t>& sectionWireframeIndices)
{
	resetZoneVisited(zones);
	resetBVHTree(root);

	sectionVertices.clear();
	sectionIndices.clear();
	sectionWireframeIndices.clear();
	QMap<Edge, uint32_t> intersectionIndexMap;
	QSet<Edge> sectionWireframes;

	clipZones(zones, plane, root, nodeVertices, intersectionIndexMap, sectionVertices, sectionIndices, sectionWireframes);

	for (const Edge& edge : sectionWireframes)
	{
		sectionWireframeIndices.append({ edge.vertices[0], edge.vertices[1] });
	}
}

void GeoUtil::fixWindingOrder(Mesh& mesh, const Face& mainFace, Face& neighborFace)
{
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			if (mainFace.vertices[i] == neighborFace.vertices[j] &&
				mainFace.vertices[(i + 1) % 3] == neighborFace.vertices[(j + 1) % 3])
			{
				flipWindingOrder(neighborFace);
				return;
			}
		}
	}
}

void GeoUtil::flipWindingOrder(Face& face)
{
	qSwap(face.vertices[0], face.vertices[1]);
}

bool GeoUtil::clipZone(const Zone& zone, const Plane& plane, QVector<NodeVertex>& nodeVertices, QMap<Edge, uint32_t>& intersectionIndexMap, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QSet<Edge>& sectionWireframes)
{
	QVector<uint32_t> intersectionIndices;
	for (int i = 0; i < zone.edgeNum; ++i)
	{
		Edge edge{ zone.edges[i * 2], zone.edges[i * 2 + 1] };
		if (intersectionIndexMap.contains(edge))
		{
			uint32_t index = intersectionIndexMap[edge];
			intersectionIndices.append(index);
		}
		else
		{
			const NodeVertex& nv0 = nodeVertices[edge.vertices[0]];
			const NodeVertex& nv1 = nodeVertices[edge.vertices[1]];
			QVector3D v0 = nv0.position;
			QVector3D v1 = nv1.position;
			QVector3D v01 = v1 - v0;
			float dnv01 = QVector3D::dotProduct(plane.normal, v01);
			if (qFuzzyIsNull(dnv01))
			{
				continue;
			}

			float t = (plane.dist - QVector3D::dotProduct(plane.normal, v0)) / dnv01;
			if (t < 0.0f || t > 1.0f)
			{
				continue;
			}

			QVector3D intersection = v0 + v01 * t;

			NodeVertex nodeVertex;
			nodeVertex.position = intersection;
			nodeVertex.totalDeformation = qLerp(nv0.totalDeformation, nv1.totalDeformation, t);

			uint32_t index = sectionVertices.count();
			intersectionIndices.append(index);
			sectionVertices.append(nodeVertex);
			intersectionIndexMap[edge] = index;
		}
	}

	if (intersectionIndices.isEmpty())
	{
		return false;
	}

	// 截面顶点排序
	const QVector3D& origin = sectionVertices[intersectionIndices.first()].position;
	std::sort(intersectionIndices.begin(), intersectionIndices.end(), [&](uint32_t a, uint32_t b) {
		QVector3D c = QVector3D::crossProduct(sectionVertices[a].position - origin, sectionVertices[b].position - origin);
		return QVector3D::dotProduct(c, plane.normal) < 0.0f;
	});

	// 构造面索引
	for (int i = 1; i < intersectionIndices.count() - 1; ++i)
	{
		sectionIndices.append({ intersectionIndices[0], intersectionIndices[i], intersectionIndices[i + 1] });
	}
	
	// 构造线索引
	for (int i = 0; i < intersectionIndices.count(); ++i)
	{
		sectionWireframes.insert({ intersectionIndices[i], intersectionIndices[(i + 1) % intersectionIndices.count()] });
	}

	return true;
}

bool GeoUtil::isManifordFace(const Mesh& mesh, const Face& face, bool strict)
{
	for (const Edge& edge : face.edges)
	{
		const auto& faces = mesh.edges[edge];
		if (faces.count() != 2 && (strict || faces.count() != 1))
		{
			return false;
		}
	}
	return true;
}

void GeoUtil::traverseMesh(Mesh& mesh)
{
	// 重置被访问属性
	resetMeshVisited(mesh);

	// 广度优先搜索所有面
	QQueue<uint32_t> queue;
	queue.enqueue(0);
	mesh.faces[0].visited = true;

	while (!queue.isEmpty())
	{
		const Face& mainFace = mesh.faces[queue.dequeue()];
		for (const Edge& edge : mainFace.edges)
		{
			for (uint32_t f : mesh.edges[edge])
			{
				Face& neighborFace = mesh.faces[f];
				if (!neighborFace.visited)
				{
					neighborFace.visited = true;
					queue.enqueue(f);
				}
			}
		}
	}
}

void GeoUtil::resetMeshVisited(Mesh& mesh)
{
	for (Face& face : mesh.faces)
	{
		face.visited = false;
	}
}

void GeoUtil::resetZoneVisited(QVector<Zone>& zones)
{
	for (Zone& zone : zones)
	{
		zone.visited = false;
	}
}

bool GeoUtil::validateMesh(Mesh& mesh)
{
	traverseMesh(mesh);

	QSet<Face> uniqueFaces;
	for (Face& face : mesh.faces)
	{
		uniqueFaces.insert(face);

		// 检测连通性
		if (!face.visited)
		{
			return false;
		}

		// 检测流形
		if (!isManifordFace(mesh, face, false))
		{
			return false;
		}
	}

	// 检测重复
	if (uniqueFaces.count() != mesh.faces.count())
	{
		return false;
	}

	return true;
}

BVHTreeNode* GeoUtil::buildBVHTree(const QVector<Zone>& zones)
{
	QVector<uint32_t> zoneIndices;
	for (int i = 0; i < zones.count(); ++i)
	{
		zoneIndices.append(i);
	}
	return buildBVHTree(zones, zoneIndices, 0, zoneIndices.count());
}

BVHTreeNode* GeoUtil::buildBVHTree(const QVector<Zone>& zones, QVector<uint32_t>& zoneIndices, int begin, int end)
{
	BVHTreeNode* node = new BVHTreeNode;
	int num = end - begin;

	if (num <= 3)
	{
		for (int i = begin; i < end; ++i)
		{
			uint32_t z = zoneIndices[i];
			const Bound& bound = zones[z].bound;
			node->bound.combine(bound);
			node->bound.cache();
		}

		for (int i = begin; i < end; ++i)
		{
			node->zones.append(zoneIndices[i]);
		}
		node->children[0] = node->children[1] = nullptr;
		node->depth = 0;
		node->isLeaf = true;
	}
	else
	{
		Bound centriodBound;
		for (int i = begin; i < end; ++i)
		{
			uint32_t z = zoneIndices[i];
			const Bound& bound = zones[z].bound;
			centriodBound.combine(bound.centriod);
		}

		int dim = centriodBound.maxDim();
		int mid = (begin + end) * 0.5f;
		std::nth_element(&zoneIndices[begin], &zoneIndices[mid], &zoneIndices[end - 1] + 1,
			[&zones, dim](uint32_t a, uint32_t b)
		{
			return zones[a].bound.centriod[dim] < zones[b].bound.centriod[dim];
		});

		int depth[2] = { 0, 0 };
		if (begin < mid)
		{
			node->children[0] = buildBVHTree(zones, zoneIndices, begin, mid);
			node->bound.combine(node->children[0]->bound);
			depth[0] = node->children[0]->depth + 1;
		}

		if (mid < end)
		{
			node->children[1] = buildBVHTree(zones, zoneIndices, mid, end);
			node->bound.combine(node->children[1]->bound);
			depth[1] = node->children[1]->depth + 1;
		}

		node->bound.cache();
		node->depth = qMax(depth[0], depth[1]);
	}

	return node;
}

void GeoUtil::interpUniformGrids(BVHTreeNode* root, UniformGrids& uniformGrids)
{

}

void GeoUtil::resetBVHTree(BVHTreeNode* node)
{
	if (!node)
	{
		return;
	}

	node->bound.reset();
	resetBVHTree(node->children[0]);
	resetBVHTree(node->children[1]);
}

void GeoUtil::clipZones(QVector<Zone>& zones, const Plane& plane, BVHTreeNode* node, QVector<NodeVertex>& nodeVertices, QMap<Edge, uint32_t>& intersectionIndexMap, QVector<NodeVertex>& sectionVertices, QVector<uint32_t>& sectionIndices, QSet<Edge>& sectionWireframes)
{
	if (node->isLeaf)
	{
		for (uint32_t z : node->zones)
		{
			Zone& zone = zones[z];
			if (!zone.visited)
			{
				zone.visited = true;
				clipZone(zone, plane, nodeVertices, intersectionIndexMap, sectionVertices, sectionIndices, sectionWireframes);
			}
		}
	}
	else
	{
		if (node->children[0]->bound.intersect(plane))
		{
			clipZones(zones, plane, node->children[0], nodeVertices, intersectionIndexMap, sectionVertices, sectionIndices, sectionWireframes);
		}
		if (node->children[1]->bound.intersect(plane))
		{
			clipZones(zones, plane, node->children[1], nodeVertices, intersectionIndexMap, sectionVertices, sectionIndices, sectionWireframes);
		}
	}
}

bool GeoUtil::isVec3NearlyEqual(const QVector3D& lhs, const QVector3D& rhs, float epsilon /*= 0.001f*/)
{
	return lhs.distanceToPoint(rhs) < epsilon;
}
