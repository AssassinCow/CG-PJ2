# 计算机图形学 Project 2：光照模型与光线追踪

本项目基于 C++ 实现了一个递归光线追踪渲染器，支持 Phong 光照、平面/三角形/变换相交、阴影投射、反射与抗锯齿。

## 目录结构

```
PJ2/
├── Project2说明.pptx       # 任务说明
├── README.md               # 本文件
└── starter2/
    ├── CMakeLists.txt
    ├── data/               # 场景与模型
    ├── sample_out/         # 参考输出
    ├── sample_solution/    # 助教提供的可执行样例
    ├── src/                # 源代码（本次实现）
    ├── vecmath/            # 向量/矩阵库
    ├── generate_images.sh  # 生成全部场景的脚本
    └── test_cases.sh
```

## 构建

项目依赖 CMake 与 C++11 编译器。

```bash
cd starter2
mkdir -p build && cd build
cmake ..
make -j
```

生成的可执行文件为 `build/a2`。

> 注：在较新版本的 GCC 下需要显式 `#include <cstdint>`，已在 `src/Octree.h` 与 `src/Image.cpp` 中补齐。

## 运行

```bash
cd starter2/build
mkdir -p out

# 场景 1-5：光线投射
./a2 -size 800 800 -input ../data/scene01_plane.txt    -output out/a01.png -normals out/a01n.png -depth 8 18 out/a01d.png
./a2 -size 800 800 -input ../data/scene02_cube.txt     -output out/a02.png -normals out/a02n.png -depth 8 18 out/a02d.png
./a2 -size 800 800 -input ../data/scene03_sphere.txt   -output out/a03.png -normals out/a03n.png -depth 8 18 out/a03d.png
./a2 -size 800 800 -input ../data/scene04_axes.txt     -output out/a04.png -normals out/a04n.png -depth 8 18 out/a04d.png
./a2 -size 800 800 -input ../data/scene05_bunny_200.txt -output out/a05.png -normals out/a05n.png -depth 0.8 1.0 out/a05d.png

# 场景 6：光追反射
./a2 -size 800 800 -input ../data/scene06_bunny_1k.txt -bounces 4 -output out/a06.png -normals out/a06n.png -depth 8 18 out/a06d.png

# 场景 7：光追 + 阴影
./a2 -size 800 800 -input ../data/scene07_arch.txt     -bounces 4 -shadows -output out/a07.png -normals out/a07n.png -depth 8 18 out/a07d.png

# 抗锯齿（jitter + 高斯滤波）
./a2 -size 300 300 -input ../data/scene04_axes.txt -jitter -filter -output out/a04_aa.png
```

### 命令行参数

| 参数 | 说明 |
|---|---|
| `-input <scene.txt>` | 场景描述文件 |
| `-output <image.png>` | 彩色输出 |
| `-size <w> <h>` | 图像尺寸 |
| `-normals <n.png>` | 法线可视化输出 |
| `-depth <min> <max> <d.png>` | 深度可视化输出 |
| `-bounces <n>` | 光追最大递归深度（`0` = 纯光线投射） |
| `-shadows` | 启用阴影射线 |
| `-jitter` | 每像素 16 次随机子像素采样 |
| `-filter` | 3× 上采样后 3×3 高斯下采样 |

## 实现要点

### 任务 1：Phong 光照

- `Light.cpp :: PointLight::getIllumination` — 输出到光源的方向、强度与距离。强度按 `I / (1 + α·d²)` 衰减（`α = _falloff`）。
- `Material.cpp :: Material::shade` — Phong 漫反射 + 镜面反射：
  - 漫反射：`k_d · max(L·N, 0)`
  - 镜面反射：`R = 2(L·N)N − L`，`k_s · max(R·E, 0)^s`，其中 `E = −ray.dir`；当 `L·N ≤ 0` 时不计算高光。
  - 两项与光强逐通道相乘。
- `Renderer.cpp :: Renderer::traceRay` — 遍历所有光源累加 `shade(...)`，最后加上环境项 `k_d ⊙ L_ambient`。

### 任务 2：光线投射

实现 `Object3D.cpp` 中以下子类的 `intersect`：

- **Plane**：平面方程 `P·n = d`，由 `t = (d − o·n) / (dir·n)` 求交；注意 `dir·n ≈ 0` 时射线与平面平行，无交点。命中时写入法向量 `n`。
- **Triangle**：采用矩阵法求重心坐标——
  ```
  [a−b  a−c  d] · [β  γ  t]ᵀ = a − o
  ```
  用 `Matrix3f::inverse()` 求解。命中条件：`α, β, γ ∈ [0,1]` 且 `t > tmin`。法线按 `α·n₀ + β·n₁ + γ·n₂` 插值（用于平滑着色）。
- **Transform**：
  - 构造时缓存 `M`、`M⁻¹`、`(M⁻¹)ᵀ`。
  - 求交前把世界空间光线经 `M⁻¹` 变换到局部坐标（方向不做归一化，保证 `t` 参数在两空间一致）。
  - 子对象返回的局部法线用 `(M⁻¹)ᵀ` 变换回世界坐标并归一化。

### 任务 3：光追与阴影

`Renderer::traceRay` 伪代码：

```
if (!scene.intersect(ray, ...)) return background(dir);

color = k_d ⊙ L_ambient                    // 环境光
for each light:
    getIllumination(...)
    if shadows and 在 [ε, distToLight] 上有命中: continue
    color += material.shade(ray, hit, toLight, intensity)

if bounces > 0 and k_s > 0:
    R = d − 2(d·N)N
    color += k_s ⊙ traceRay(Ray(p + εR, R), ε, bounces − 1, ...)

return color
```

阴影/反射射线都加 `1e-3` 的偏移以避免自相交。背景用 `SceneParser::getBackgroundColor(dir)`（支持 CubeMap）。

### 拓展：抗锯齿

- **抖动采样（`-jitter`）**：每个像素在 `[−0.5, 0.5]` 范围随机偏移 16 次，求算术平均。
- **高斯滤波（`-filter`）**：以 3× 分辨率渲染整图，然后用 σ=1 的 3×3 离散高斯核对相邻 9 个像素加权合并为 1 像素。核值：

  ```
  (1/16) * | 1 2 1 |
           | 2 4 2 |
           | 1 2 1 |
  ```

  同时对两者组合使用可获得更平滑的边缘，参见 `out/a04_aa.png` 与 `out/a04_noaa.png` 的对比。

## 输出对照

成功通过 `sample_out/` 中参考图的比对：

| 场景 | 描述 |
|---|---|
| scene01 | 球 + 平面，Phong 光照 |
| scene02 | 立方体（三角网格） |
| scene03 | Transform 缩放球（椭球） |
| scene04 | Transform 坐标轴 |
| scene05 | 200 面 Stanford Bunny |
| scene06 | 1k 面 Bunny + 光追反射（CubeMap 背景） |
| scene07 | Arch + 阴影 + 反射 |

## 提交

将 `starter2/` 目录、`build/a2`（Linux 可执行文件）与 PDF 报告打包提交。
