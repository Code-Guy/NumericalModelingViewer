#include "geoutil.h"
#include <QQueue>

void GeoUtil::fixWindingOrder(Mesh& mesh)
{
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

	QVector<Face> isolatedFaces;
	QVector<Face> linkedFaces;
	for (const Face& face : mesh.faces)
	{
		if (!face.visited)
		{
			isolatedFaces.append(face);
		}
		else
		{
			linkedFaces.append(face);
		}
	}

	//mesh.faces = isolatedFaces;
	mesh.faces = linkedFaces;
}

QVector<ClipLine> GeoUtil::clipMesh(Mesh& mesh, const Plane& plane)
{
	QVector<ClipLine> polylines;

	for (Face& face : mesh.faces)
	{
		face.visited = false;
	}

	while (true)
	{
		Edge clippedEdge;
		bool findClippedEdge = false;
		for (Face& face : mesh.faces)
		{
			if (face.visited)
			{
				continue;
			}

			for (const Edge& edge : face.edges)
			{
				QVector3D intersection;
				if (clipEdge(mesh, edge, plane, intersection))
				{
					face.visited = true;
					clippedEdge = edge;
					break;
				}
			}

			if (findClippedEdge)
			{
				break;
			}
		}

		if (!findClippedEdge)
		{
			break;
		}

		while (true)
		{
			
		}
	}

	QQueue<uint32_t> queue;
	queue.enqueue(0);
	while (!queue.isEmpty())
	{
		const Face& mainFace = mesh.faces[queue.dequeue()];

	}

	return polylines;
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

	float t = (plane.d - QVector3D::dotProduct(plane.normal, v0)) / dnv01;
	if (t < 0.0f || t > 1.0f)
	{
		return false;
	}

	intersection = v0 + v01 * t;
	return true;
}
