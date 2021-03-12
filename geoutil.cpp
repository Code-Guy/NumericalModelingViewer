#include "geoutil.h"
#include <QQueue>

void GeoUtil::cleanMesh(Mesh& mesh)
{
	traverseMesh(mesh);

	// 删除不合法（重复 || 孤立 || 非流形）面
	QSet<Face> uniqueFaces;
	QVector<Face> invalidFaces;
	QVector<Face> validFaces;
	QMap<Edge, QVector<uint32_t>> validEdges;
	int validFaceIndex = 0;

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
			invalidFaces.append(face);
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

	//mesh.faces = invalidFaces;
	mesh.faces = validFaces;
	mesh.edges = validEdges;
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

QVector<ClipLine> GeoUtil::clipMesh(Mesh& mesh, const Plane& plane)
{
	QVector<ClipLine> clipLines;
	resetMeshVisited(mesh);

	while (true)
	{
		ClipLine clipLine;
		Edge startEdge;
		QPair<Edge, QVector3D> hit;
		for (Face& face : mesh.faces)
		{
			if (face.visited)
			{
				continue;
			}

			if (clipFace(mesh, face, plane, hit))
			{
				startEdge = hit.first;
				clipLine.vertices.push_back(hit.second);
				break;
			}
		}

		if (clipLine.vertices.isEmpty())
		{
			break;
		}

		QQueue<QPair<Edge, bool>> queue;
		queue.enqueue({ startEdge, true });
		while (!queue.isEmpty())
		{
			QPair<Edge, bool> pair = queue.dequeue();
			const Edge& edge = pair.first;
			bool positive = pair.second;
			for (uint32_t f : mesh.edges[edge])
			{
				Face& face = mesh.faces[f];
				if (!face.visited)
				{
					face.visited = true;
					if (clipFace(mesh, face, plane, hit, &edge, false))
					{
						const Edge& edge = hit.first;
						const QVector3D& intersection = hit.second;
						if (positive)
						{
							if (intersection.distanceToPoint(clipLine.vertices.back()) > 0.001f)
							{
								clipLine.vertices.push_back(intersection);
							}
						}
						else
						{
							if (intersection.distanceToPoint(clipLine.vertices.front()) > 0.001f)
							{
								clipLine.vertices.push_front(intersection);
							}
						}

						queue.enqueue({ edge, positive });
						positive = !positive;
					}
				}
			}
		}

		clipLines.append(clipLine);
	}

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

bool GeoUtil::clipFace(const Mesh& mesh, const Face& face, const Plane& plane, QPair<Edge, QVector3D>& hit, const Edge* exceptEdge /*= nullptr*/, bool exceptBoundaryEdge /*= true*/)
{
	for (const Edge& edge : face.edges)
	{
		if ((exceptEdge && edge == *exceptEdge) || (isBoundaryEdge(mesh, edge) && exceptBoundaryEdge))
		{
			continue;
		}

		QVector3D intersection;
		if (clipEdge(mesh, edge, plane, intersection))
		{
			hit = { edge, intersection };
			return true;
		}
	}
	return false;
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
