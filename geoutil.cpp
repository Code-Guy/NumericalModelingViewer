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

	for (int i = 0; i < mesh.faces.count(); ++i)
	{
		Face& face = mesh.faces[i];
		bool existed = uniqueFaces.contains(face);
		if (!existed)
		{
			uniqueFaces.insert(face);
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
	flipWindingOrder(mesh.faces[0]);
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

QVector<ClipLine> GeoUtil::clipMesh(Mesh& mesh, const Plane& plane, BVHTreeNode* root)
{
	QVector<ClipLine> clipLines;
	resetMeshVisited(mesh);
	resetBVHTree(root);

	QElapsedTimer profileTimer;
	profileTimer.start();
	QMap<Edge, QVector3D> hits;
	findAllClipEdges(mesh, plane, root, hits);
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
								if (!isVec3NearlyEqual(intersection, clipLine.vertices.back()))
								{
									clipLine.vertices.push_back(intersection);
								}
							}
							else
							{
								if (!isVec3NearlyEqual(intersection, clipLine.vertices.front()))
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

void GeoUtil::clipFace(const Mesh& mesh, const Face& face, const Plane& plane, QMap<Edge, QVector3D>& hits)
{
	for (const Edge& edge : face.edges)
	{
		if (hits.contains(edge))
		{
			continue;
		}

		QVector3D intersection;
		if (clipEdge(mesh, edge, plane, intersection))
		{
			hits[edge] = intersection;
		}
	}
}

bool GeoUtil::clipEdge(const Mesh& mesh, const Edge& edge, const Plane& plane, QVector3D& intersection)
{
	QVector3D v0 = mesh.vertices[edge.vertices[0]];
	QVector3D v1 = mesh.vertices[edge.vertices[1]];
	QVector3D v01 = v1 - v0;
	float dnv01 = QVector3D::dotProduct(plane.normal, v01);
	if (qFuzzyIsNull(dnv01))
	{
		return false;
	}

	float t = (plane.dist - QVector3D::dotProduct(plane.normal, v0)) / dnv01;
	if (t < 0.0f || t > 1.0f)
	{
		return false;
	}

	intersection = v0 + v01 * t;
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

bool GeoUtil::isBoundaryEdge(const Mesh& mesh, const Edge& edge)
{
	return mesh.edges[edge].count() == 1;
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
	node->num = num;

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

		node->children[0] = buildBVHTree(mesh, faces, begin, mid);
		node->children[1] = buildBVHTree(mesh, faces, mid, end);
		node->bound.combine(node->children[0]->bound);
		node->bound.combine(node->children[1]->bound);
		node->bound.cache();
		node->isLeaf = false;
	}

	return node;
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

void GeoUtil::findAllClipEdges(Mesh& mesh, const Plane& plane, BVHTreeNode* node, QMap<Edge, QVector3D>& hits)
{
	if (node->isLeaf)
	{
		for (uint32_t f : node->faces)
		{
			Face& face = mesh.faces[f];
			if (!face.visited)
			{
				face.visited = true;
				clipFace(mesh, mesh.faces[f], plane, hits);
			}
		}
	}
	else
	{
		if (node->children[0]->bound.intersect(plane))
		{
			findAllClipEdges(mesh, plane, node->children[0], hits);
		}
		if (node->children[1]->bound.intersect(plane))
		{
			findAllClipEdges(mesh, plane, node->children[1], hits);
		}
	}
}

bool GeoUtil::isVec3NearlyEqual(const QVector3D& lhs, const QVector3D& rhs, float epsilon /*= 0.001f*/)
{
	return lhs.distanceToPoint(rhs) < epsilon;
}
