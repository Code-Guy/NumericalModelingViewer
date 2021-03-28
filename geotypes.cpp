#include "geotypes.h"

QVector3D Bound::size()
{
	return max - min;
}

// Bound成员函数实现
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

bool Bound::contain(const QVector3D& point) const
{
	return point[0] >= min[0] && point[0] <= max[0] &&
		point[1] >= min[1] && point[1] <= max[1] &&
		point[2] >= min[2] && point[2] <= max[2];
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

bool Bound::calcPointPlaneSide(const QVector3D& point, const Plane& plane, float epsilon)
{
	return QVector3D::dotProduct(point, plane.normal) > plane.dist;
}

// Zone类成员函数实现

void Zone::cache(const QVector<NodeVertex>& nodeVertices)
{
	for (int i = 0; i < vertexNum; ++i)
	{
		values[i] = nodeVertices[vertices[i]].totalDeformation;
	}
	if (type == Wedge)
	{
		values[6] = values[3];
		values[7] = values[5];
	}
	else if (type == Pyramid)
	{
		values[5] = values[6] = values[7] = values[3];
	}
	else if (type == DegeneratedBrick)
	{
		values[7] = values[6];
	}
	else if (type == Tetrahedron)
	{
		values[4] = values[2];
		values[5] = values[6] = values[7] = values[3];
	}

	static const int xi[8] = { 0, 2, 4, 6, 1, 3, 5, 7 };
	static const int yi[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	static const int zi[8] = { 0, 1, 5, 4, 2, 3, 7, 6 };

	for (int i = 0; i < 4; ++i)
	{
		valueBound.min[0] = qMin(valueBound.min[0], nodeVertices[vertices[xi[i]]].position[0]);
		valueBound.max[0] = qMax(valueBound.max[0], nodeVertices[vertices[xi[i + 4]]].position[0]);

		valueBound.min[1] = qMin(valueBound.min[1], nodeVertices[vertices[yi[i]]].position[1]);
		valueBound.max[1] = qMax(valueBound.max[1], nodeVertices[vertices[yi[i + 4]]].position[1]);

		valueBound.min[2] = qMin(valueBound.min[2], nodeVertices[vertices[zi[i]]].position[2]);
		valueBound.max[2] = qMax(valueBound.max[2], nodeVertices[vertices[zi[i + 4]]].position[2]);
	}
}

bool Zone::interp(const QVector3D& point, float& value) const
{
	if (!bound.contain(point))
	{
		return false;
	}

	float xt = (point[0] - valueBound.min[0]) / (valueBound.max[0] - valueBound.min[0]);
	float yt = (point[1] - valueBound.min[1]) / (valueBound.max[1] - valueBound.min[1]);
	float zt = (point[2] - valueBound.min[2]) / (valueBound.max[2] - valueBound.min[2]);

	value = qTriLerp(values[0], values[1], values[2], values[3],
		values[4], values[5], values[6], values[7],
		xt, yt, zt);
	return true;
}