#include "geotypes.h"

Plane::Plane(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2)
{
	origin = v0;
	QVector3D v01 = v1 - v0;
	QVector3D v02 = v2 - v0;
	if (qIsNearlyZero(v01) || qIsNearlyZero(v02) || qIsNearlyEqual(v01, v02))
	{
		degenerated = true;
		return;
	}

	normal = QVector3D::crossProduct(v01, v02).normalized();
	dist = QVector3D::dotProduct(origin, normal);
}

void Plane::normalize()
{
	normal.normalize();
	dist = QVector3D::dotProduct(origin, normal);
}

bool Plane::checkSide(const QVector3D& point) const
{
	return QVector3D::dotProduct(point, normal) > dist;
}

QVector3D Bound::size() const
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

	bool first = plane.checkSide(corners[0]);
	for (int i = 1; i < 8; i++)
	{
		bool other = plane.checkSide(corners[i]);
		if (other != first)
		{
			intersectFlag = 1;
			return true;
		}
	}

	intersectFlag = 0;
	return false;
}

bool Bound::intersect(const Ray& ray)
{
	double tx1 = (min.x() - ray.origin.x()) * ray.invDirection.x();
	double tx2 = (max.x() - ray.origin.x()) * ray.invDirection.x();

	double tmin = qMin(tx1, tx2);
	double tmax = qMax(tx1, tx2);

	double ty1 = (min.y() - ray.origin.y()) * ray.invDirection.y();
	double ty2 = (max.y() - ray.origin.y()) * ray.invDirection.y();

	tmin = qMax(tmin, qMin(ty1, ty2));
	tmax = qMin(tmax, qMax(ty1, ty2));

	return tmax >= tmin;
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

int Zone::facetID = 0;
bool Zone::isValid() const
{
	QSet<uint32_t> vertexSet;
	for (int i = 0; i < vertexNum; ++i)
	{
		vertexSet.insert(vertices[i]);
	}
	return vertexSet.count() > 3;
}

// Zone类成员函数实现
void Zone::cache(const QVector<NodeVertex>& nodeVertices)
{
	// 记录单元每个点的数值
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

	// 记录单元的每个面
	if (type == Brick || type == DegeneratedBrick)
	{
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[2]].position, nodeVertices[vertices[1]].position));
		planes.append(Plane(nodeVertices[vertices[3]].position, nodeVertices[vertices[6]].position, nodeVertices[vertices[5]].position));
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[3]].position, nodeVertices[vertices[2]].position));
		planes.append(Plane(nodeVertices[vertices[1]].position, nodeVertices[vertices[4]].position, nodeVertices[vertices[6]].position));
		planes.append(Plane(nodeVertices[vertices[2]].position, nodeVertices[vertices[5]].position, nodeVertices[vertices[4]].position));
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[1]].position, nodeVertices[vertices[3]].position));
	}
	else if (type == Wedge || type == Pyramid)
	{
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[2]].position, nodeVertices[vertices[1]].position));
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[3]].position, nodeVertices[vertices[2]].position));
		planes.append(Plane(nodeVertices[vertices[1]].position, nodeVertices[vertices[4]].position, nodeVertices[vertices[6]].position));
		planes.append(Plane(nodeVertices[vertices[2]].position, nodeVertices[vertices[5]].position, nodeVertices[vertices[4]].position));
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[1]].position, nodeVertices[vertices[3]].position));
	}
	else if (type == Tetrahedron)
	{
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[2]].position, nodeVertices[vertices[1]].position));
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[3]].position, nodeVertices[vertices[2]].position));
		planes.append(Plane(nodeVertices[vertices[1]].position, nodeVertices[vertices[4]].position, nodeVertices[vertices[6]].position));
		planes.append(Plane(nodeVertices[vertices[0]].position, nodeVertices[vertices[1]].position, nodeVertices[vertices[3]].position));
	}

	// 记录单元的基向量
	origin = nodeVertices[vertices[0]].position;
	QVector3D axis[3];
	for (int i = 0; i < 3; ++i)
	{
		axis[i] = nodeVertices[vertices[i + 1]].position - origin;
	}
	QMatrix4x4 basisMatrix(axis[0][0], axis[1][0], axis[2][0], 0.0f,
		axis[0][1], axis[1][1], axis[2][1], 0.0f,
		axis[0][2], axis[1][2], axis[2][2], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	bool invertible = false;
	invertedBasisMatrix = basisMatrix.inverted(&invertible);
	if (!invertible)
	{
		qDebug() << "Invertible!";
	}
}

bool Zone::contain(const QVector3D& point) const
{
	bool first = planes[0].checkSide(point);
	for (int i = 1; i < planes.count(); ++i)
	{
		bool other = planes[i].checkSide(point);
		if (other != first)
		{
			return false;
		}
	}
	return true;
}

bool Zone::interp(const QVector3D& point, float& value) const
{
	if (!contain(point))
	{
		return false;
	}

	QVector4D t = invertedBasisMatrix * QVector4D(point - origin, 0.0f);
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

float qMaxDimVal(const QVector3D& v)
{
	return v[qMaxDim(v)];
}

bool qIsNearlyZero(const QVector3D& v0, float epsilon /*= 0.001f*/)
{
	return qIsNearlyEqual(v0, QVector3D(0.0f, 0.0f, 0.0f), epsilon);
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

QSet<Edge> Facet::getEdges() const
{
	if (type == T3)
	{
		return { {indices[0], indices[1]}, {indices[1], indices[2]}, {indices[2], indices[0]} };
	}
	else if (type == Q4)
	{
		return { {indices[0], indices[1]}, { indices[1], indices[2] }, { indices[2], indices[3]}, { indices[3], indices[0] } };
	}
	else
	{
		return {};
	}
}

bool Facet::intersect(const QVector<NodeVertex>& nodeVertices, const Ray& ray, float& t) const
{
	if (intersect(nodeVertices, indices[0], indices[1], indices[2], ray, t))
	{
		return true;
	}
	if (type == Q4 && intersect(nodeVertices, indices[1], indices[2], indices[3], ray, t))
	{
		return true;
	}
	return false;
}

bool Facet::intersect(const QVector<NodeVertex>& nodeVertices, quint32 i0, quint32 i1, quint32 i2, const Ray& ray, float& t) const
{
	QVector3D v0v1 = nodeVertices[i1].position - nodeVertices[i0].position;
	QVector3D v0v2 = nodeVertices[i2].position - nodeVertices[i0].position;
	QVector3D pvec = QVector3D::crossProduct(ray.direction, v0v2);
	float det = QVector3D::dotProduct(v0v1, pvec);

	// ray and triangle are parallel if det is close to 0
	if (fabs(det) < 0.00001f) return false;

	float invDet = 1 / det;
	float u, v;

	QVector3D tvec = ray.origin - nodeVertices[i0].position;
	u = QVector3D::dotProduct(tvec, pvec) * invDet;
	const float kEpsilon = 0.0f;
	if (u < -kEpsilon || u > 1.0f + kEpsilon) return false;

	QVector3D qvec = QVector3D::crossProduct(tvec, v0v1);
	v = QVector3D::dotProduct(ray.direction, qvec) * invDet;
	if (v < -kEpsilon || u + v > 1.0f + kEpsilon) return false;

	t = QVector3D::dotProduct(v0v2, qvec) * invDet;
	return true;
}
