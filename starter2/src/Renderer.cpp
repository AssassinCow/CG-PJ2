#include "Renderer.h"

#include "ArgParser.h"
#include "Camera.h"
#include "Image.h"
#include "Ray.h"
#include "VecUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>


Renderer::Renderer(const ArgParser &args) :
    _args(args),
    _scene(args.input_file)
{
}

namespace {

// Uniform [-0.5, 0.5] jitter offset (in pixel units).
inline float jitterOffset()
{
    return (std::rand() / (float)RAND_MAX) - 0.5f;
}

// 3x3 Gaussian kernel with sigma=1 (discrete, normalized)
// Standard values for sigma=1 centered 3x3.
const float kGauss3[3][3] = {
    {1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f},
    {2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f},
    {1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f}
};

// Downsample a high-res image (wk x hk) to (w x h) with k=3 via Gaussian filter.
Image gaussianDownsample(const Image &src, int w, int h)
{
    const int k = 3;
    Image out(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // center pixel in the high-res image
            int cx = x * k + 1;
            int cy = y * k + 1;
            Vector3f sum(0.0f);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int sx = std::min(std::max(cx + dx, 0), src.getWidth()  - 1);
                    int sy = std::min(std::max(cy + dy, 0), src.getHeight() - 1);
                    sum += kGauss3[dy + 1][dx + 1] * src.getPixel(sx, sy);
                }
            }
            out.setPixel(x, y, sum);
        }
    }
    return out;
}

} // anonymous namespace


void
Renderer::Render()
{
    int w = _args.width;
    int h = _args.height;

    // If -filter is requested, render at 3x resolution then downsample
    const int k = _args.filter ? 3 : 1;
    const int renderW = w * k;
    const int renderH = h * k;

    // Number of samples per pixel (when jitter is enabled)
    const int numSamples = _args.jitter ? 16 : 1;

    Image image(renderW, renderH);
    Image nimage(renderW, renderH);
    Image dimage(renderW, renderH);

    Camera* cam = _scene.getCamera();
    for (int y = 0; y < renderH; ++y) {
        for (int x = 0; x < renderW; ++x) {
            Vector3f colorSum(0.0f);
            Vector3f normalSum(0.0f);
            float tSum = 0.0f;
            int validT = 0;

            for (int s = 0; s < numSamples; ++s) {
                float dx = 0.0f, dy = 0.0f;
                if (_args.jitter) {
                    dx = jitterOffset();
                    dy = jitterOffset();
                }

                float ndcx = 2 * ((x + dx) / (renderW - 1.0f)) - 1.0f;
                float ndcy = 2 * ((y + dy) / (renderH - 1.0f)) - 1.0f;

                Ray r = cam->generateRay(Vector2f(ndcx, ndcy));
                Hit hh;
                Vector3f color = traceRay(r, cam->getTMin(), _args.bounces, hh);

                colorSum  += color;
                normalSum += (hh.getNormal() + 1.0f) / 2.0f;
                if (hh.getT() < std::numeric_limits<float>::max()) {
                    tSum += hh.t;
                    ++validT;
                }
            }

            Vector3f color  = colorSum  / (float)numSamples;
            Vector3f normal = normalSum / (float)numSamples;

            image.setPixel(x, y, color);
            nimage.setPixel(x, y, normal);

            float range = (_args.depth_max - _args.depth_min);
            if (range) {
                float avgT = (validT > 0) ? (tSum / validT)
                                          : std::numeric_limits<float>::max();
                dimage.setPixel(x, y, Vector3f((avgT - _args.depth_min) / range));
            }
        }
    }

    // Downsample via Gaussian if -filter is on
    Image finalImage  = _args.filter ? gaussianDownsample(image,  w, h) : image;
    Image finalNormal = _args.filter ? gaussianDownsample(nimage, w, h) : nimage;
    Image finalDepth  = _args.filter ? gaussianDownsample(dimage, w, h) : dimage;

    // save the files
    if (_args.output_file.size()) {
        finalImage.savePNG(_args.output_file);
    }
    if (_args.depth_file.size()) {
        finalDepth.savePNG(_args.depth_file);
    }
    if (_args.normals_file.size()) {
        finalNormal.savePNG(_args.normals_file);
    }
}



Vector3f
Renderer::traceRay(const Ray &r,
    float tmin,
    int bounces,
    Hit &h) const
{
    if (!_scene.getGroup()->intersect(r, tmin, h)) {
        return _scene.getBackgroundColor(r.getDirection());
    }

    // Hit point and geometry info
    Vector3f hitPoint = r.pointAtParameter(h.getT());
    Vector3f N        = h.getNormal().normalized();
    Material *mat     = h.getMaterial();

    // Start with ambient contribution
    const Vector3f &ambientL = _scene.getAmbientLight();
    const Vector3f &kd       = mat->getDiffuseColor();
    Vector3f color(kd[0] * ambientL[0],
                   kd[1] * ambientL[1],
                   kd[2] * ambientL[2]);

    // Direct illumination from each light
    for (int i = 0; i < _scene.getNumLights(); ++i) {
        Light *light = _scene.getLight(i);
        Vector3f toLight;
        Vector3f intensity;
        float distToLight;
        light->getIllumination(hitPoint, toLight, intensity, distToLight);

        // Shadow test
        if (_args.shadows) {
            Ray shadowRay(hitPoint + 1e-3f * toLight, toLight);
            Hit shadowHit;
            // any hit with t < distToLight means occlusion
            if (_scene.getGroup()->intersect(shadowRay, 1e-3f, shadowHit)) {
                if (shadowHit.getT() < distToLight) {
                    continue;
                }
            }
        }

        color += mat->shade(r, h, toLight, intensity);
    }

    // Recursive reflection for specular materials
    if (bounces > 0) {
        const Vector3f &ks = mat->getSpecularColor();
        if (ks[0] > 0.0f || ks[1] > 0.0f || ks[2] > 0.0f) {
            Vector3f d = r.getDirection().normalized();
            // Reflection direction: r_refl = d - 2(d . N) N
            Vector3f reflDir = d - 2.0f * Vector3f::dot(d, N) * N;
            reflDir.normalize();

            Ray reflRay(hitPoint + 1e-3f * reflDir, reflDir);
            Hit reflHit;
            Vector3f reflColor = traceRay(reflRay, 1e-3f, bounces - 1, reflHit);

            color += Vector3f(ks[0] * reflColor[0],
                              ks[1] * reflColor[1],
                              ks[2] * reflColor[2]);
        }
    }

    return color;
}
