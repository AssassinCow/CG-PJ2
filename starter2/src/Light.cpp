#include "Light.h"
    void DirectionalLight::getIllumination(const Vector3f &p,
                                 Vector3f &tolight,
                                 Vector3f &intensity,
                                 float &distToLight) const
    {
        // the direction to the light is the opposite of the
        // direction of the directional light source

        // BEGIN STARTER
        tolight = -_direction;
        intensity  = _color;
        distToLight = std::numeric_limits<float>::max();
        // END STARTER
    }
    void PointLight::getIllumination(const Vector3f &p,
                                 Vector3f &tolight,
                                 Vector3f &intensity,
                                 float &distToLight) const
    {
        // Direction from surface point to light
        Vector3f diff = _position - p;
        distToLight = diff.abs();
        tolight = diff / distToLight;

        // Intensity attenuation: I / (1 + falloff * d^2)
        float attenuation = 1.0f / (1.0f + _falloff * distToLight * distToLight);
        intensity = _color * attenuation;
    }

