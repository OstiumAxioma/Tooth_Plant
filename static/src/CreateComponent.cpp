#include "CreateComponent.h"

#include <algorithm>
#include <cmath>

#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkCylinderSource.h>
#include <vtkMath.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyLine.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTriangle.h>
#include <vtkTubeFilter.h>

namespace CreateComponent {

class ComponentCreator::Impl {
public:
    double startPoint[3]{0.0, 0.0, 0.0};
    double endPoint[3]{0.0, 0.0, 1.0};
    double neckHeight{-1.0}; // 截锥
    double bodyHeight{-1.0}; // 颈部圆柱
    double headHeight{-1.0};
    double baseTopRadius{-1.0};
    int resolution{32};
    double threadDepth{0.0};
    int threadTurns{0};

    vtkSmartPointer<vtkActor> actor;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
};

ComponentCreator::ComponentCreator() : pImpl(std::make_unique<Impl>()) {}
ComponentCreator::~ComponentCreator() = default;

void ComponentCreator::SetStartPoint(double x, double y, double z) {
    pImpl->startPoint[0] = x;
    pImpl->startPoint[1] = y;
    pImpl->startPoint[2] = z;
}

void ComponentCreator::SetEndPoint(double x, double y, double z) {
    pImpl->endPoint[0] = x;
    pImpl->endPoint[1] = y;
    pImpl->endPoint[2] = z;
}

void ComponentCreator::SetNeckHeight(double height) { pImpl->neckHeight = height; }
void ComponentCreator::SetBodyHeight(double height) { pImpl->bodyHeight = height; }
void ComponentCreator::SetHeadHeight(double height) { pImpl->headHeight = height; }
void ComponentCreator::SetBaseTopRadius(double radius) { pImpl->baseTopRadius = radius; }
void ComponentCreator::SetResolution(int resolution) { pImpl->resolution = resolution; }
void ComponentCreator::SetThreadDepth(double depth) { pImpl->threadDepth = depth; }
void ComponentCreator::SetThreadTurns(int turns) { pImpl->threadTurns = turns; }

namespace {

struct Basis {
    double n[3]; // 轴向（start->end）
    double u[3]; // 与 n 垂直的单位向量
    double v[3]; // u×n
};

Basis MakeBasis(const double dir[3]) {
    Basis b{};
    b.n[0] = dir[0]; b.n[1] = dir[1]; b.n[2] = dir[2];
    double tmp[3] = { std::abs(b.n[0]) < 0.9 ? 1.0 : 0.0, std::abs(b.n[0]) < 0.9 ? 0.0 : 1.0, 0.0 };
    vtkMath::Cross(b.n, tmp, b.v);
    vtkMath::Normalize(b.v);
    vtkMath::Cross(b.v, b.n, b.u);
    return b;
}

// 沿 start->end 轴直接生成截锥（start 为顶，上大下小）
vtkSmartPointer<vtkPolyData> BuildFrustumWorld(double topRadius, double bottomRadius, double height, int resolution,
                                              const double start[3], const Basis& basis) {
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    const double full = vtkMath::Pi() * 2.0;

    auto addPoint = [&](double r, double z, double angle) {
        double c = std::cos(angle), s = std::sin(angle);
        double x = start[0] + basis.n[0] * z + basis.u[0] * r * c + basis.v[0] * r * s;
        double y = start[1] + basis.n[1] * z + basis.u[1] * r * c + basis.v[1] * r * s;
        double zc = start[2] + basis.n[2] * z + basis.u[2] * r * c + basis.v[2] * r * s;
        points->InsertNextPoint(x, y, zc);
    };

    for (int i = 0; i < resolution; ++i) {
        double angle = full * i / resolution;
        addPoint(topRadius, 0.0, angle);
        addPoint(bottomRadius, height, angle);
    }

    vtkIdType topCenterId = points->InsertNextPoint(start);
    double bottomCenter[3] = { start[0] + basis.n[0] * height,
                               start[1] + basis.n[1] * height,
                               start[2] + basis.n[2] * height };
    vtkIdType bottomCenterId = points->InsertNextPoint(bottomCenter);

    auto addTri = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
        auto tri = vtkSmartPointer<vtkTriangle>::New();
        tri->GetPointIds()->SetId(0, a);
        tri->GetPointIds()->SetId(1, b);
        tri->GetPointIds()->SetId(2, c);
        polys->InsertNextCell(tri);
    };

    for (int i = 0; i < resolution; ++i) {
        vtkIdType t0 = 2 * i;
        vtkIdType b0 = t0 + 1;
        vtkIdType t1 = 2 * ((i + 1) % resolution);
        vtkIdType b1 = t1 + 1;

        addTri(t0, t1, b1);
        addTri(t0, b1, b0);
        addTri(topCenterId, t0, t1);
        addTri(bottomCenterId, b1, b0);
    }

    auto poly = vtkSmartPointer<vtkPolyData>::New();
    poly->SetPoints(points);
    poly->SetPolys(polys);
    return poly;
}

// 沿轴向生成圆柱（顶面在 z0，底面在 z0+height）
vtkSmartPointer<vtkPolyData> BuildCylinderWorld(double radius, double height, int resolution, double z0,
                                                const double start[3], const Basis& basis) {
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    const double full = vtkMath::Pi() * 2.0;

    auto addPoint = [&](double r, double z, double angle) {
        double c = std::cos(angle), s = std::sin(angle);
        double x = start[0] + basis.n[0] * z + basis.u[0] * r * c + basis.v[0] * r * s;
        double y = start[1] + basis.n[1] * z + basis.u[1] * r * c + basis.v[1] * r * s;
        double zc = start[2] + basis.n[2] * z + basis.u[2] * r * c + basis.v[2] * r * s;
        points->InsertNextPoint(x, y, zc);
    };

    for (int i = 0; i < resolution; ++i) {
        double angle = full * i / resolution;
        addPoint(radius, z0, angle);
        addPoint(radius, z0 + height, angle);
    }

    double topCenter[3] = { start[0] + basis.n[0] * z0,
                            start[1] + basis.n[1] * z0,
                            start[2] + basis.n[2] * z0 };
    double bottomCenter[3] = { start[0] + basis.n[0] * (z0 + height),
                               start[1] + basis.n[1] * (z0 + height),
                               start[2] + basis.n[2] * (z0 + height) };
    vtkIdType topCenterId = points->InsertNextPoint(topCenter);
    vtkIdType bottomCenterId = points->InsertNextPoint(bottomCenter);

    auto addTri = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
        auto tri = vtkSmartPointer<vtkTriangle>::New();
        tri->GetPointIds()->SetId(0, a);
        tri->GetPointIds()->SetId(1, b);
        tri->GetPointIds()->SetId(2, c);
        polys->InsertNextCell(tri);
    };

    for (int i = 0; i < resolution; ++i) {
        vtkIdType t0 = 2 * i;
        vtkIdType b0 = t0 + 1;
        vtkIdType t1 = 2 * ((i + 1) % resolution);
        vtkIdType b1 = t1 + 1;

        addTri(t0, t1, b1);
        addTri(t0, b1, b0);
        addTri(topCenterId, t0, t1);
        addTri(bottomCenterId, b1, b0);
    }

    auto poly = vtkSmartPointer<vtkPolyData>::New();
    poly->SetPoints(points);
    poly->SetPolys(polys);
    return poly;
}

vtkSmartPointer<vtkPolyData> BuildThreadedCylinderWorld(double radius, double depth, double height, int turns,
                                                        int resolution, double z0,
                                                        const double start[3], const Basis& basis) {
    if (turns <= 0 || depth <= 0.0) {
        return BuildCylinderWorld(radius, height, resolution, z0, start, basis);
    }

    int resTheta = std::max(16, resolution);
    int resZ = std::max(resolution * turns * 2, turns * 16);
    double full = vtkMath::Pi() * 2.0;
    double pitch = height / static_cast<double>(turns);
    double flatGap = 0.25;       // 两端纯平区长度（固定长度，对称）
    double fadeLen = 0.2;      // 渐变长度（固定长度，对称）

    // 若颈部太短，按比例缩放空隙/渐变，但保持两端对称
    double needed = 2.0 * (flatGap + fadeLen);
    if (needed >= height) {
        double scale = height / needed;
        flatGap *= scale;
        fadeLen *= scale;
    }

    double fadeInStart = flatGap;
    double fadeInEnd   = flatGap + fadeLen;
    double fadeOutStart= height - (flatGap + fadeLen);
    double fadeOutEnd  = height - flatGap;

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto polys = vtkSmartPointer<vtkCellArray>::New();

    auto radiusAt = [&](double zLocal, double theta) {
        if (zLocal < flatGap || zLocal > fadeOutEnd) {
            return radius; // 纯平区
        }

        double phase = (zLocal / pitch) - (theta / full);
        double wave = std::sin(full * phase);

        double fade = 1.0;
        if (zLocal < fadeInEnd) {
            fade = std::clamp((zLocal - fadeInStart) / fadeLen, 0.0, 1.0);
        } else if (zLocal > fadeOutStart) {
            fade = std::clamp((fadeOutEnd - zLocal) / fadeLen, 0.0, 1.0);
        }

        return radius + depth * wave * fade; // 起伏并在端部渐隐
    };

    auto pointId = [&](int iz, int it) {
        int idx = iz * resTheta + (it % resTheta);
        return static_cast<vtkIdType>(idx);
    };

    // 生成点
    for (int iz = 0; iz <= resZ; ++iz) {
        double tz = static_cast<double>(iz) / resZ;
        double z = z0 + height * tz;
        for (int it = 0; it < resTheta; ++it) {
            double theta = full * it / resTheta;
            double r = radiusAt(z - z0, theta);
            double c = std::cos(theta), s = std::sin(theta);
            double x = start[0] + basis.n[0] * z + basis.u[0] * r * c + basis.v[0] * r * s;
            double y = start[1] + basis.n[1] * z + basis.u[1] * r * c + basis.v[1] * r * s;
            double zc = start[2] + basis.n[2] * z + basis.u[2] * r * c + basis.v[2] * r * s;
            points->InsertNextPoint(x, y, zc);
        }
    }

    // 侧面三角
    for (int iz = 0; iz < resZ; ++iz) {
        for (int it = 0; it < resTheta; ++it) {
            vtkIdType p00 = pointId(iz, it);
            vtkIdType p01 = pointId(iz, it + 1);
            vtkIdType p10 = pointId(iz + 1, it);
            vtkIdType p11 = pointId(iz + 1, it + 1);

            auto tri1 = vtkSmartPointer<vtkTriangle>::New();
            tri1->GetPointIds()->SetId(0, p00);
            tri1->GetPointIds()->SetId(1, p01);
            tri1->GetPointIds()->SetId(2, p11);
            polys->InsertNextCell(tri1);

            auto tri2 = vtkSmartPointer<vtkTriangle>::New();
            tri2->GetPointIds()->SetId(0, p00);
            tri2->GetPointIds()->SetId(1, p11);
            tri2->GetPointIds()->SetId(2, p10);
            polys->InsertNextCell(tri2);
        }
    }

    // 顶盖/底盖使用平均半径封闭
    auto addCap = [&](bool top) {
        int iz = top ? resZ : 0;
        double z = z0 + height * (top ? 1.0 : 0.0);
        double avgR = radius; // 取基准半径封盖
        double center[3] = { start[0] + basis.n[0] * z,
                             start[1] + basis.n[1] * z,
                             start[2] + basis.n[2] * z };
        vtkIdType centerId = points->InsertNextPoint(center);
        for (int it = 0; it < resTheta; ++it) {
            vtkIdType p0 = pointId(iz, it);
            vtkIdType p1 = pointId(iz, it + 1);
            auto tri = vtkSmartPointer<vtkTriangle>::New();
            if (top) {
                tri->GetPointIds()->SetId(0, centerId);
                tri->GetPointIds()->SetId(1, p1);
                tri->GetPointIds()->SetId(2, p0);
            } else {
                tri->GetPointIds()->SetId(0, centerId);
                tri->GetPointIds()->SetId(1, p0);
                tri->GetPointIds()->SetId(2, p1);
            }
            polys->InsertNextCell(tri);
        }
    };
    addCap(true);
    addCap(false);

    auto poly = vtkSmartPointer<vtkPolyData>::New();
    poly->SetPoints(points);
    poly->SetPolys(polys);
    return poly;
}

vtkSmartPointer<vtkPolyData> BuildHemisphereWorld(double radius, double height, int resolution, double z0,
                                                 const double start[3], const Basis& basis) {
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    const double full = vtkMath::Pi() * 2.0;

    int thetaRes = std::max(12, resolution * 2);
    int phiRes = std::max(8, resolution);

    for (int ip = 0; ip <= phiRes; ++ip) {
        double phi = (vtkMath::Pi() * 0.5) * ip / phiRes; // 0..pi/2
        double rxy = radius * std::cos(phi);
        double zlocal = height * std::sin(phi); // 压缩后高度
        for (int it = 0; it < thetaRes; ++it) {
            double theta = full * it / thetaRes;
            double c = std::cos(theta), s = std::sin(theta);
            double x = start[0] + basis.n[0] * (z0 + zlocal) + basis.u[0] * rxy * c + basis.v[0] * rxy * s;
            double y = start[1] + basis.n[1] * (z0 + zlocal) + basis.u[1] * rxy * c + basis.v[1] * rxy * s;
            double z = start[2] + basis.n[2] * (z0 + zlocal) + basis.u[2] * rxy * c + basis.v[2] * rxy * s;
            points->InsertNextPoint(x, y, z);
        }
    }

    auto addCell = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
        auto tri = vtkSmartPointer<vtkTriangle>::New();
        tri->GetPointIds()->SetId(0, a);
        tri->GetPointIds()->SetId(1, b);
        tri->GetPointIds()->SetId(2, c);
        polys->InsertNextCell(tri);
    };

    auto idx = [&](int ip, int it) {
        int thetaResLocal = thetaRes;
        it = it % thetaResLocal;
        return static_cast<vtkIdType>(ip * thetaResLocal + it);
    };

    for (int ip = 0; ip < phiRes; ++ip) {
        for (int it = 0; it < thetaRes; ++it) {
            vtkIdType p00 = idx(ip, it);
            vtkIdType p01 = idx(ip, it + 1);
            vtkIdType p10 = idx(ip + 1, it);
            vtkIdType p11 = idx(ip + 1, it + 1);
            addCell(p00, p01, p11);
            addCell(p00, p11, p10);
        }
    }

    auto poly = vtkSmartPointer<vtkPolyData>::New();
    poly->SetPoints(points);
    poly->SetPolys(polys);
    return poly;
}

// 不再使用（改为直接生成带螺纹的颈部网格）
vtkSmartPointer<vtkPolyData> BuildThreadWorld(double radius, double depth, double height, int turns, int resolution,
                                             double z0, const double start[3], const Basis& basis) {
    auto empty = vtkSmartPointer<vtkPolyData>::New();
    return empty;
}

} // namespace

bool ComponentCreator::BuildActor(double radius, int resolution) {
    double dir[3] = {
        pImpl->endPoint[0] - pImpl->startPoint[0],
        pImpl->endPoint[1] - pImpl->startPoint[1],
        pImpl->endPoint[2] - pImpl->startPoint[2]
    };

    double length = vtkMath::Norm(dir);
    if (length <= 1e-6 || radius <= 1e-6) {
        return false;
    }

    vtkMath::Normalize(dir);

    int segments = std::max(8, (pImpl->resolution > 3 ? pImpl->resolution : resolution));

    // 高度：如果未设置，给出固定默认值（不再依赖总长）
    double neckH = pImpl->neckHeight > 0.0 ? pImpl->neckHeight : 1.0; // 截锥
    double bodyH = pImpl->bodyHeight > 0.0 ? pImpl->bodyHeight : 2.0; // 圆柱
    double headH = pImpl->headHeight > 0.0 ? pImpl->headHeight : 1.0;

    if (neckH <= 1e-6 || bodyH <= 1e-6 || headH <= 1e-6) {
        return false;
    }

    double topRadius = pImpl->baseTopRadius > radius ? pImpl->baseTopRadius : radius * 1.1;
    if (topRadius <= radius) {
        topRadius = radius * 1.05;
    }

    Basis basis = MakeBasis(dir);

    // 在世界坐标直接构建三段，轴向即 start->end
    double threadDepth = pImpl->threadDepth > 0.0 ? pImpl->threadDepth : 0.1;
    int threadTurns = pImpl->threadTurns > 0 ? pImpl->threadTurns : 20;

    auto neck = BuildFrustumWorld(topRadius, radius, neckH, segments, pImpl->startPoint, basis);
    auto body = (threadDepth > 0.0 && threadTurns > 0)
        ? BuildThreadedCylinderWorld(radius, threadDepth, bodyH, threadTurns, segments, neckH, pImpl->startPoint, basis)
        : BuildCylinderWorld(radius, bodyH, segments, neckH, pImpl->startPoint, basis);
    auto head = BuildHemisphereWorld(radius, headH, segments, neckH + bodyH, pImpl->startPoint, basis);

    auto append = vtkSmartPointer<vtkAppendPolyData>::New();
    append->AddInputData(neck);
    append->AddInputData(body);
    append->AddInputData(head);
    append->Update();

    auto clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputConnection(append->GetOutputPort());
    clean->Update();

    pImpl->mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    pImpl->mapper->SetInputConnection(clean->GetOutputPort());

    pImpl->actor = vtkSmartPointer<vtkActor>::New();
    pImpl->actor->SetMapper(pImpl->mapper);

    return true;
}

vtkActor* ComponentCreator::GetActor() const { return pImpl->actor; }

} // namespace CreateComponent
