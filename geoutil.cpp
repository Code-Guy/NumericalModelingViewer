#include "geoutil.h"
#include <QQueue>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QtAlgorithms>
#include <algorithm>
#include <fstream>

// GeoUtil成员函数实现
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
	face.bound.cache();
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

void GeoUtil::pickZone(QVector<Zone>& zones, const Ray& ray, BVHTreeNode* root, const QVector<NodeVertex>& nodeVertices, QVector<uint32_t>& pickIndices, bool pickZoneMode)
{
	resetZoneVisited(zones);
	resetBVHTree(root);

	pickIndices.clear();
	QMap<float, QSet<Edge>> pickEdgesMap;

	pickZone(zones, ray, root, nodeVertices, pickEdgesMap, pickZoneMode);

	if (!pickEdgesMap.empty())
	{
		for (const auto& iter : pickEdgesMap.cbegin().value())
		{
			pickIndices.append({ iter.vertices[0], iter.vertices[1] });
		}
	}
}

QVector<ClipLine> GeoUtil::genIsolines(Mesh& mesh, QVector<NodeVertex>& nodeVertices, float value, BVHTreeNode* root)
{
	QVector<ClipLine> clipLines;
	resetMeshVisited(mesh);
	resetBVHTree(root);

	QElapsedTimer profileTimer;
	profileTimer.start();
	QMap<Edge, QVector3D> hits;
	findAllIsoEdges(mesh, nodeVertices, value, root, hits);
	resetMeshVisited(mesh);
	qint64 t0 = profileTimer.restart();

	while (!hits.isEmpty())
	{
		ClipLine clipLine;
		QQueue<QPair<Edge, bool>> queue;
		queue.enqueue({ hits.begin().key(), true });
		clipLine.vertices.push_front(hits.begin().value());
		hits.remove(hits.begin().key());

		while (!queue.isEmpty())
		{
			QPair<Edge, bool> pair = queue.dequeue();
			const Edge& mainEdge = pair.first;
			bool positive = pair.second;
			for (uint32_t f : mesh.edges[mainEdge])
			{
				Face& neighborFace = mesh.faces[f];
				if (!neighborFace.visited)
				{
					neighborFace.visited = true;

					for (const Edge& neighborEdge : neighborFace.edges)
					{
						if (!(neighborEdge == mainEdge) && hits.contains(neighborEdge))
						{
							QVector3D intersection = hits[neighborEdge];
							if (positive)
							{
								if (!qIsNearlyEqual(intersection, clipLine.vertices.back()))
								{
									clipLine.vertices.push_back(intersection);
								}
							}
							else
							{
								if (!qIsNearlyEqual(intersection, clipLine.vertices.front()))
								{
									clipLine.vertices.push_front(intersection);
								}
							}

							hits.remove(neighborEdge);
							queue.enqueue({ neighborEdge, positive });
							positive = !positive;

							break;
						}
					}
				}
			}
		}

		clipLines.append(clipLine);
	}

	qint64 t1 = profileTimer.restart();
	//qDebug() << t0 << t1;

	return clipLines;
}

void GeoUtil::findAllIsoEdges(Mesh& mesh, QVector<NodeVertex>& nodeVertices, float value, BVHTreeNode* node, QMap<Edge, QVector3D>& hits)
{
	if (node->isLeaf)
	{
		for (uint32_t f : node->faces)
		{
			Face& face = mesh.faces[f];
			if (!face.visited)
			{
				face.visited = true;
				findIsoEdges(mesh, nodeVertices, mesh.faces[f], value, hits);
			}
		}
	}
	else
	{
		QVector3D isoPoint(value, value, value);
		if (node->children[0]->bound.contain(isoPoint))
		{
			findAllIsoEdges(mesh, nodeVertices, value, node->children[0], hits);
		}
		if (node->children[1]->bound.contain(isoPoint))
		{
			findAllIsoEdges(mesh, nodeVertices, value, node->children[1], hits);
		}
	}
}

void GeoUtil::findIsoEdges(const Mesh& mesh, QVector<NodeVertex>& nodeVertices, const Face& face, float value, QMap<Edge, QVector3D>& hits)
{
	for (const Edge& edge : face.edges)
	{
		if (hits.contains(edge))
		{
			continue;
		}

		const NodeVertex& nv0 = nodeVertices[edge.vertices[0]];
		const NodeVertex& nv1 = nodeVertices[edge.vertices[1]];
		if ((value - nv0.totalDeformation) * (value - nv1.totalDeformation) <= 0)
		{
			QVector3D intersection = qLerp(nv0.position, nv1.position, (value - nv0.totalDeformation) / (nv1.totalDeformation - nv0.totalDeformation));
			hits[edge] = intersection;
		}
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

bool GeoUtil::interpZones(const QVector<Zone>& zones, BVHTreeNode* node, const QVector3D& point, float& value)
{
	if (node->isLeaf)
	{
		for (uint32_t z : node->zones)
		{
			const Zone& zone = zones[z];
			if (zone.interp(point, value))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		if (node->children[0]->bound.contain(point) && interpZones(zones, node->children[0], point, value))
		{
			return true;
		}
		if (node->children[1]->bound.contain(point) && interpZones(zones, node->children[1], point, value))
		{
			return true;
		}

		return false;
	}
}

bool GeoUtil::inZones(const QVector<Zone>& zones, BVHTreeNode* node, const QVector3D& point)
{
	if (node->isLeaf)
	{
		for (uint32_t z : node->zones)
		{
			const Zone& zone = zones[z];
			if (zone.contain(point))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		if (node->children[0]->bound.contain(point) && inZones(zones, node->children[0], point))
		{
			return true;
		}
		if (node->children[1]->bound.contain(point) && inZones(zones, node->children[1], point))
		{
			return true;
		}

		return false;
	}
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

BVHTreeNode* GeoUtil::buildBVHTree(const Mesh& mesh)
{
	QVector<uint32_t> faces;
	for (int i = 0; i < mesh.faces.count(); ++i)
	{
		faces.append(i);
	}
	return buildBVHTree(mesh, faces, 0, mesh.faces.count());
}

BVHTreeNode* GeoUtil::buildBVHTree(const Mesh& mesh, QVector<uint32_t>& faces, int begin, int end)
{
	BVHTreeNode* node = new BVHTreeNode;
	int num = end - begin;

	if (num <= 3)
	{
		for (int i = begin; i < end; ++i)
		{
			uint32_t f = faces[i];
			const Bound& bound = mesh.faces[f].bound;
			node->bound.combine(bound);
			node->bound.cache();
		}

		for (int i = begin; i < end; ++i)
		{
			node->faces.append(faces[i]);
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
			uint32_t f = faces[i];
			const Bound& bound = mesh.faces[f].bound;
			centriodBound.combine(bound.centriod);
		}

		int dim = centriodBound.maxDim();
		int mid = (begin + end) * 0.5f;
		std::nth_element(&faces[begin], &faces[mid], &faces[end - 1] + 1,
			[&mesh, dim](uint32_t a, uint32_t b)
		{
			return mesh.faces[a].bound.centriod[dim] < mesh.faces[b].bound.centriod[dim];
		});

		int depth[2] = { 0, 0 };
		if (begin < mid)
		{
			node->children[0] = buildBVHTree(mesh, faces, begin, mid);
			node->bound.combine(node->children[0]->bound);
			depth[0] = node->children[0]->depth + 1;
		}

		if (mid < end)
		{
			node->children[1] = buildBVHTree(mesh, faces, mid, end);
			node->bound.combine(node->children[1]->bound);
			depth[1] = node->children[1]->depth + 1;
		}

		node->bound.cache();
		node->depth = qMax(depth[0], depth[1]);
	}

	return node;
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

void GeoUtil::destroyBVHTree(BVHTreeNode* root)
{
	if (root)
	{
		destroyBVHTree(root->children[0]);
		destroyBVHTree(root->children[1]);
		SAFE_DELETE(root);
	}
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

void GeoUtil::pickZone(QVector<Zone>& zones, const Ray& ray, BVHTreeNode* node, const QVector<NodeVertex>& nodeVertices, QMap<float, QSet<Edge>>& pickEdgesMap, bool pickZoneMode)
{
	if (node->isLeaf)
	{
		for (uint32_t z : node->zones)
		{
			Zone& zone = zones[z];
			if (!zone.visited)
			{
				zone.visited = true;

				if (pickZoneMode)
				{
					for (const Facet& facet : zone.facets)
					{
						float t;
						QSet<Edge> pickEdges;
						if (facet.intersect(nodeVertices, ray, t))
						{
							float minT = 1e8f;
							for (const Facet& f : zone.facets)
							{
								if (f.intersect(nodeVertices, ray, t))
								{
									minT = qMin(minT, t);
								}
								
								pickEdges.unite(f.getEdges());
							}

							pickEdgesMap[minT] = pickEdges;
							break;
						}
					}
				}
				else
				{
					for (const Facet& facet : zone.facets)
					{
						float t;
						if (facet.intersect(nodeVertices, ray, t))
						{
							pickEdgesMap[t] = facet.getEdges();
						}
					}
				}
			}
		}
	}
	else
	{
		if (node->children[0]->bound.intersect(ray))
		{
			pickZone(zones, ray, node->children[0], nodeVertices, pickEdgesMap, pickZoneMode);
		}
		if (node->children[1]->bound.intersect(ray))
		{
			pickZone(zones, ray, node->children[1], nodeVertices, pickEdgesMap, pickZoneMode);
		}
	}
}
