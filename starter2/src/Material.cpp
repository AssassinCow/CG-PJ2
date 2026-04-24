#include "Material.h"
Vector3f Material::shade(const Ray &ray,
    const Hit &hit,
    const Vector3f &dirToLight,
    const Vector3f &lightIntensity)
{
    Vector3f N = hit.getNormal().normalized();
    Vector3f L = dirToLight.normalized();

    // Diffuse term: k_d * max(L . N, 0) * I
    float diffuseFactor = std::max(0.0f, Vector3f::dot(L, N));
    Vector3f diffuse = diffuseFactor * _diffuseColor;

    // Specular term: k_s * max(R . E, 0)^s * I
    // R = 2 (L . N) N - L, E = -ray direction (from surface toward camera)
    Vector3f specular(0.0f);
    if (diffuseFactor > 0.0f) {
        Vector3f R = 2.0f * Vector3f::dot(L, N) * N - L;
        R.normalize();
        Vector3f E = -ray.getDirection().normalized();
        float specFactor = std::max(0.0f, Vector3f::dot(R, E));
        specular = std::pow(specFactor, _shininess) * _specularColor;
    }

    // Component-wise multiply with light intensity
    Vector3f color = diffuse + specular;
    return Vector3f(color[0] * lightIntensity[0],
                    color[1] * lightIntensity[1],
                    color[2] * lightIntensity[2]);
}
