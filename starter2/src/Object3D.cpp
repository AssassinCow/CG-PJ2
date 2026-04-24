#include "Object3D.h"

bool Sphere::intersect(const Ray &r, float tmin, Hit &h) const
{
    // BEGIN STARTER

    // We provide sphere intersection code for you.
    // You should model other intersection implementations after this one.

    // Locate intersection point ( 2 pts )
    const Vector3f &rayOrigin = r.getOrigin(); //Ray origin in the world coordinate
    const Vector3f &dir = r.getDirection();

    Vector3f origin = rayOrigin - _center;      //Ray origin in the sphere coordinate

    float a = dir.absSquared();
    float b = 2 * Vector3f::dot(dir, origin);
    float c = origin.absSquared() - _radius * _radius;

    // no intersection
    if (b * b - 4 * a * c < 0) {
        return false;
    }

    float d = sqrt(b * b - 4 * a * c);

    float tplus = (-b + d) / (2.0f*a);
    float tminus = (-b - d) / (2.0f*a);

    // the two intersections are at the camera back
    if ((tplus < tmin) && (tminus < tmin)) {
        return false;
    }

    float t = 10000;
    // the two intersections are at the camera front
    if (tminus > tmin) {
        t = tminus;
    }

    // one intersection at the front. one at the back 
    if ((tplus > tmin) && (tminus < tmin)) {
        t = tplus;
    }

    if (t < h.getT()) {
        Vector3f normal = r.pointAtParameter(t) - _center;
        normal = normal.normalized();
        h.set(t, this->material, normal);
        return true;
    }
    // END STARTER
    return false;
}

// Add object to group
void Group::addObject(Object3D *obj) {
    m_members.push_back(obj);
}

// Return number of objects in group
int Group::getGroupSize() const {
    return (int)m_members.size();
}

bool Group::intersect(const Ray &r, float tmin, Hit &h) const
{
    // BEGIN STARTER
    // we implemented this for you
    bool hit = false;
    for (Object3D* o : m_members) {
        if (o->intersect(r, tmin, h)) {
            hit = true;
        }
    }
    return hit;
    // END STARTER
}


Plane::Plane(const Vector3f &normal, float d, Material *m) : Object3D(m) {
    _normal = normal.normalized();
    _d = d;
}
bool Plane::intersect(const Ray &r, float tmin, Hit &h) const
{
    // Plane: P . n = d
    // Ray: P = o + t*d_r
    // t = (d - o . n) / (d_r . n)
    const Vector3f &o = r.getOrigin();
    const Vector3f &dir = r.getDirection();

    float denom = Vector3f::dot(dir, _normal);
    if (std::fabs(denom) < 1e-8f) {
        return false; // ray parallel to plane
    }

    float t = (_d - Vector3f::dot(o, _normal)) / denom;
    if (t < tmin || t >= h.getT()) {
        return false;
    }

    h.set(t, this->material, _normal);
    return true;
}
bool Triangle::intersect(const Ray &r, float tmin, Hit &hit) const
{
    // Moller-Trumbore: solve [ -d | v1-v0 | v2-v0 ] * [t, beta, gamma]^T = (o - v0)
    const Vector3f &o = r.getOrigin();
    const Vector3f &d = r.getDirection();

    const Vector3f &a = _v[0];
    const Vector3f &b = _v[1];
    const Vector3f &c = _v[2];

    // Matrix A: columns are [-d, b - a, c - a] => but Matrix3f takes columns or rows
    // We solve A * x = (o - a) where A = [a-b, a-c, d] for barycentric per slide hint.
    // Use classic form: columns are [a-b, a-c, d], x = [beta, gamma, t]
    Vector3f col0 = a - b;
    Vector3f col1 = a - c;
    Vector3f col2 = d;
    Matrix3f A(col0, col1, col2, true);
    Vector3f rhs = a - o;

    bool singular = false;
    Matrix3f Ainv = A.inverse(&singular, 1e-8f);
    if (singular) {
        return false;
    }

    Vector3f x = Ainv * rhs;
    float beta  = x[0];
    float gamma = x[1];
    float t     = x[2];
    float alpha = 1.0f - beta - gamma;

    if (t < tmin || t >= hit.getT()) {
        return false;
    }
    if (beta < 0.0f || gamma < 0.0f || alpha < 0.0f) {
        return false;
    }
    if (beta > 1.0f || gamma > 1.0f || alpha > 1.0f) {
        return false;
    }

    // Interpolate normal using barycentric coords
    Vector3f n = alpha * _normals[0] + beta * _normals[1] + gamma * _normals[2];
    n = n.normalized();
    hit.set(t, this->material, n);
    return true;
}


Transform::Transform(const Matrix4f &m,
    Object3D *obj) : _object(obj) {
    _M = m;
    _Minv = m.inverse();
    _MinvT = _Minv.transposed();
}
bool Transform::intersect(const Ray &r, float tmin, Hit &h) const
{
    // Transform ray from world to object coordinates
    Vector3f localOrigin = (_Minv * Vector4f(r.getOrigin(), 1.0f)).xyz();
    Vector3f localDir    = (_Minv * Vector4f(r.getDirection(), 0.0f)).xyz();

    // Do NOT normalize localDir — we need t to be consistent in world space
    Ray localRay(localOrigin, localDir);

    // Intersect with child. The t stored in h is already world-t from earlier
    // hits (other objects), which matches because the ray direction in local
    // space shares the same scalar parameterization as the world ray.
    float oldT = h.getT();
    if (_object->intersect(localRay, tmin, h)) {
        // Transform the normal back to world coordinates
        Vector3f localNormal = h.getNormal();
        Vector3f worldNormal = (_MinvT * Vector4f(localNormal, 0.0f)).xyz().normalized();
        h.set(h.getT(), h.getMaterial(), worldNormal);
        return true;
    }
    (void)oldT;
    return false;
}