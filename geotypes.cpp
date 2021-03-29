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
	return qMaxDim(min, max);
}

int Bound::matchCorner(const QVector3D& position)
{
	for (int i = 0; i < 8; ++i)
	{
		if (qIsNearlyEqual(corners[i], position))
		{
			return i;
		}
	}
	return -1;
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
	centriod = (min + max) * 0.5f;
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
}

bool Zone::interp(const QVector3D& point, float& value) const
{
	if (!bound.contain(point))
	{
		return false;
	}

	float xt = (point[0] - bound.min[0]) / (bound.max[0] - bound.min[0]);
	float yt = (point[1] - bound.min[1]) / (bound.max[1] - bound.min[1]);
	float zt = (point[2] - bound.min[2]) / (bound.max[2] - bound.min[2]);

	value = qTriLerp(values[0], values[1], values[2], values[3],
		values[4], values[5], values[6], values[7],
		xt, yt, zt);
	return true;
}

int qMaxDim(const QVector3D& v0, const QVector3D& v1)
{
	QVector3D v01 = v1 - v0;
	if (v01[0] > v01[1] && v01[0] > v01[2])
	{
		return 0;
	}
	if (v01[1] > v01[0] && v01[1] > v01[2])
	{
		return 1;
	}
	return 2;
}

bool qIsNearlyEqual(const QVector3D& v0, const QVector3D& v1, float epsilon /*= 0.001f*/)
{
	QVector3D v01 = v1 - v0;
	return qAbs(v01[0]) < epsilon && qAbs(v01[1]) < epsilon && qAbs(v01[2]) < epsilon;
}
