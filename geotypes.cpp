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

Bound Bound::scaled(float s) const
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
	return qMaxDim(max - min);
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

	origin = nodeVertices[vertices[0]].position;
	for (int i = 0; i < 3; ++i)
	{
		QVector3D rawAxis = nodeVertices[vertices[i + 1]].position - origin;
		axis[i] = QVector3D(0.0f, 0.0f, 0.0f);
		int maxDim = qMaxDim(rawAxis);
		axis[i][maxDim] = rawAxis[maxDim];
	}
}

bool Zone::interp(const QVector3D& point, float& value) const
{
	if (!bound.contain(point))
	{
		return false;
	}

	float t[3];
	QVector3D originToPoint = point - origin;
	for (int i = 0; i < 3; ++i)
	{
		t[i] = QVector3D::dotProduct(originToPoint, axis[i]) / axis[i].lengthSquared();
	}

	value = qTriLerp(values[0], values[1], values[2], values[4],
		values[3], values[6], values[5], values[7],
		t[0], t[1], t[2]);
	return true;
}

int qMaxDim(const QVector3D& v)
{
	if (qAbs(v[0]) > qAbs(v[1]) && qAbs(v[0]) > qAbs(v[2]))
	{
		return 0;
	}
	if (qAbs(v[1]) > qAbs(v[0]) && qAbs(v[1]) > qAbs(v[2]))
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

float qManhattaDistance(const QVector3D& v0, const QVector3D& v1)
{
	QVector3D v01 = v1 - v0;
	return qAbs(v01[0]) + qAbs(v01[1]) + qAbs(v01[2]);
}

QVector3D qToVec3(const std::array<double, 3>& arr3)
{
	return QVector3D(arr3[0], arr3[1], arr3[2]);
}

std::array<double, 3> qToArr3(const QVector3D& vec3)
{
	return std::array<double, 3>{vec3[0], vec3[1], vec3[2]};
}