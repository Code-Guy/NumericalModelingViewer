#include "geoutil.h"
#include <QQueue>

void GeoUtil::fixWindingOrder(Mesh& mesh)
{
	QQueue<uint32_t> queue;
	queue.enqueue(0);
	mesh.faces[0].visited = true;

	int num = 1;
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
					num++;
					queue.enqueue(f);
				}
			}
		}
	}

	for (Face& face : mesh.faces)
	{
		face.visited = false;
	}

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
					
					for (int i = 0; i < 3; ++i)
					{
						for (int j = 0; j < 3; ++j)
						{
							if (mainFace.vertices[i] == neighborFace.vertices[j] &&
								mainFace.vertices[(i + 1) % 3] == neighborFace.vertices[(j + 1) % 3])
							{
								qDebug() << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
							}
						}
					}

					queue.enqueue(f);
				}
			}
		}
	}
}

QVector<ClipLine> GeoUtil::clipMesh(const Mesh& mesh, const Plane& plane)
{
	QVector<ClipLine> polylines;

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
				qSwap(neighborFace.vertices[0], neighborFace.vertices[1]);
				return;
			}
		}
	}
}