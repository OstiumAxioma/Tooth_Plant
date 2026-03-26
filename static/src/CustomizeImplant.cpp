#include "CustomizeImplant.h"

#include <algorithm>
#include <cmath>

#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkMath.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkSmartPointer.h>
#include <vtkTriangle.h>
#include <QtGlobal>
#include <vtkSTLWriter.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

namespace {

    struct Basis {
        double n[3];
        double u[3];
        double v[3];
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

    Basis MakeBasisWithReference(const double dir[3], const double reference[3]) {
        Basis b{};
        b.n[0] = dir[0]; b.n[1] = dir[1]; b.n[2] = dir[2];
        vtkMath::Normalize(b.n);

        double refProj[3] = {
            reference[0] - vtkMath::Dot(reference, b.n) * b.n[0],
            reference[1] - vtkMath::Dot(reference, b.n) * b.n[1],
            reference[2] - vtkMath::Dot(reference, b.n) * b.n[2]
        };
        if (vtkMath::Norm(refProj) <= 1e-6) {
            return MakeBasis(b.n);
        }
        vtkMath::Normalize(refProj);
        b.u[0] = refProj[0]; b.u[1] = refProj[1]; b.u[2] = refProj[2];
        vtkMath::Cross(b.n, b.u, b.v);
        vtkMath::Normalize(b.v);
        return b;
    }

    Basis MakeBasisFromPrevious(const double dir[3], const Basis& previous) {
        double projectedU[3] = {
            previous.u[0] - vtkMath::Dot(previous.u, dir) * dir[0],
            previous.u[1] - vtkMath::Dot(previous.u, dir) * dir[1],
            previous.u[2] - vtkMath::Dot(previous.u, dir) * dir[2]
        };
        if (vtkMath::Norm(projectedU) > 1e-6) return MakeBasisWithReference(dir, previous.u);
        double projectedV[3] = {
            previous.v[0] - vtkMath::Dot(previous.v, dir) * dir[0],
            previous.v[1] - vtkMath::Dot(previous.v, dir) * dir[1],
            previous.v[2] - vtkMath::Dot(previous.v, dir) * dir[2]
        };
        if (vtkMath::Norm(projectedV) > 1e-6) return MakeBasisWithReference(dir, previous.v);
        return MakeBasis(dir);
    }

    struct CircleFrame {
        double center[3];
        Basis basis;
        double radius{ 1.0 };
    };

    double DegreesToRadians(double angle) {
        return angle * vtkMath::Pi() / 180.0;
    }

    bool SavePolyDataToFile(vtkPolyData* polyData, const std::string& path) {
        if (!polyData || path.empty()) return false;

        double bounds[6];
        polyData->GetBounds(bounds);
        const double oldCenter[3] = {
            (bounds[0] + bounds[1]) / 2.0,
            (bounds[2] + bounds[3]) / 2.0,
            (bounds[4] + bounds[5]) / 2.0
        };
        auto transform = vtkSmartPointer<vtkTransform>::New();
        transform->Translate(-oldCenter[0], -oldCenter[1], -oldCenter[2]);

        auto transformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        transformFilter->SetInputData(polyData);
        transformFilter->SetTransform(transform);
        transformFilter->Update();

        auto stlWriter = vtkSmartPointer<vtkSTLWriter>::New();
        stlWriter->SetFileName(path.c_str());
        stlWriter->SetInputConnection(transformFilter->GetOutputPort());
        return stlWriter->Write() == 1;
    }

    // ---- 圆盘（端盖）----
    vtkSmartPointer<vtkPolyData> BuildDiskWorld(const CircleFrame& frame, int resolution, bool reverseWinding) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto polys  = vtkSmartPointer<vtkCellArray>::New();
        const double full = vtkMath::Pi() * 2.0;

        for (int i = 0; i < resolution; ++i) {
            const double angle = full * i / resolution;
            const double c = std::cos(angle), s = std::sin(angle);
            points->InsertNextPoint(
                frame.center[0] + frame.basis.u[0] * frame.radius * c + frame.basis.v[0] * frame.radius * s,
                frame.center[1] + frame.basis.u[1] * frame.radius * c + frame.basis.v[1] * frame.radius * s,
                frame.center[2] + frame.basis.u[2] * frame.radius * c + frame.basis.v[2] * frame.radius * s
            );
        }
        const vtkIdType centerId = points->InsertNextPoint(frame.center);
        for (int i = 0; i < resolution; ++i) {
            const vtkIdType p0 = i, p1 = (i + 1) % resolution;
            auto tri = vtkSmartPointer<vtkTriangle>::New();
            tri->GetPointIds()->SetId(0, centerId);
            if (reverseWinding) { tri->GetPointIds()->SetId(1, p1); tri->GetPointIds()->SetId(2, p0); }
            else                { tri->GetPointIds()->SetId(1, p0); tri->GetPointIds()->SetId(2, p1); }
            polys->InsertNextCell(tri);
        }
        auto poly = vtkSmartPointer<vtkPolyData>::New();
        poly->SetPoints(points); poly->SetPolys(polys);
        return poly;
    }

    // ---- Loft 侧壁 ----
    vtkSmartPointer<vtkPolyData> BuildLoftWallWorld(const CircleFrame& bottom, const CircleFrame& top, int resolution) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto polys  = vtkSmartPointer<vtkCellArray>::New();
        const double full = vtkMath::Pi() * 2.0;

        for (int i = 0; i < resolution; ++i) {
            const double angle = full * i / resolution;
            const double c = std::cos(angle), s = std::sin(angle);
            points->InsertNextPoint(
                bottom.center[0] + bottom.basis.u[0] * bottom.radius * c + bottom.basis.v[0] * bottom.radius * s,
                bottom.center[1] + bottom.basis.u[1] * bottom.radius * c + bottom.basis.v[1] * bottom.radius * s,
                bottom.center[2] + bottom.basis.u[2] * bottom.radius * c + bottom.basis.v[2] * bottom.radius * s
            );
            points->InsertNextPoint(
                top.center[0] + top.basis.u[0] * top.radius * c + top.basis.v[0] * top.radius * s,
                top.center[1] + top.basis.u[1] * top.radius * c + top.basis.v[1] * top.radius * s,
                top.center[2] + top.basis.u[2] * top.radius * c + top.basis.v[2] * top.radius * s
            );
        }
        auto addTri = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
            auto tri = vtkSmartPointer<vtkTriangle>::New();
            tri->GetPointIds()->SetId(0, a); tri->GetPointIds()->SetId(1, b); tri->GetPointIds()->SetId(2, c);
            polys->InsertNextCell(tri);
        };
        for (int i = 0; i < resolution; ++i) {
            vtkIdType b0 = 2 * i, t0 = b0 + 1;
            vtkIdType b1 = 2 * ((i + 1) % resolution), t1 = b1 + 1;
            addTri(b0, b1, t1); addTri(b0, t1, t0);
        }
        auto poly = vtkSmartPointer<vtkPolyData>::New();
        poly->SetPoints(points); poly->SetPolys(polys);
        return poly;
    }

    // ---- 圆锥台 ----
    vtkSmartPointer<vtkPolyData> BuildFrustumWorld(double topRadius, double bottomRadius, double height,
        int resolution, const double start[3], const Basis& basis) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto polys  = vtkSmartPointer<vtkCellArray>::New();
        const double full = vtkMath::Pi() * 2.0;

        auto addPoint = [&](double r, double z, double angle) {
            double c = std::cos(angle), s = std::sin(angle);
            points->InsertNextPoint(
                start[0] + basis.n[0] * z + basis.u[0] * r * c + basis.v[0] * r * s,
                start[1] + basis.n[1] * z + basis.u[1] * r * c + basis.v[1] * r * s,
                start[2] + basis.n[2] * z + basis.u[2] * r * c + basis.v[2] * r * s
            );
        };
        for (int i = 0; i < resolution; ++i) {
            double angle = full * i / resolution;
            addPoint(topRadius, 0.0, angle);
            addPoint(bottomRadius, height, angle);
        }
        vtkIdType topCenterId = points->InsertNextPoint(start);
        double bc[3] = { start[0] + basis.n[0]*height, start[1] + basis.n[1]*height, start[2] + basis.n[2]*height };
        vtkIdType bottomCenterId = points->InsertNextPoint(bc);

        auto addTri = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
            auto tri = vtkSmartPointer<vtkTriangle>::New();
            tri->GetPointIds()->SetId(0,a); tri->GetPointIds()->SetId(1,b); tri->GetPointIds()->SetId(2,c);
            polys->InsertNextCell(tri);
        };
        for (int i = 0; i < resolution; ++i) {
            vtkIdType t0 = 2*i, b0 = t0+1, t1 = 2*((i+1)%resolution), b1 = t1+1;
            addTri(t0,t1,b1); addTri(t0,b1,b0);
            addTri(topCenterId,t0,t1); addTri(bottomCenterId,b1,b0);
        }
        auto poly = vtkSmartPointer<vtkPolyData>::New();
        poly->SetPoints(points); poly->SetPolys(polys);
        return poly;
    }

    // ---- 圆柱体（支持内径，形成环形顶盖）----
    vtkSmartPointer<vtkPolyData> BuildCylinderWorld(double radius, double innerRadius, double height,
        int resolution, double z0, const double start[3], const Basis& basis) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto polys  = vtkSmartPointer<vtkCellArray>::New();
        const double full = vtkMath::Pi() * 2.0;

        auto addPoint = [&](double r, double z, double angle) {
            double c = std::cos(angle), s = std::sin(angle);
            points->InsertNextPoint(
                start[0] + basis.n[0]*z + basis.u[0]*r*c + basis.v[0]*r*s,
                start[1] + basis.n[1]*z + basis.u[1]*r*c + basis.v[1]*r*s,
                start[2] + basis.n[2]*z + basis.u[2]*r*c + basis.v[2]*r*s
            );
        };
        for (int i = 0; i < resolution; ++i) {
            double angle = full * i / resolution;
            addPoint(radius, z0, angle);
            addPoint(radius, z0 + height, angle);
        }

        // 顶盖：有内径则环形，否则实心
        vtkIdType topCenterId = -1;
        vtkIdType* innerTopIds = nullptr;
        if (innerRadius > 0.0) {
            innerTopIds = new vtkIdType[resolution];
            for (int i = 0; i < resolution; ++i) {
                double angle = full * i / resolution;
                double c = std::cos(angle), s = std::sin(angle);
                innerTopIds[i] = points->InsertNextPoint(
                    start[0] + basis.n[0]*z0 + basis.u[0]*innerRadius*c + basis.v[0]*innerRadius*s,
                    start[1] + basis.n[1]*z0 + basis.u[1]*innerRadius*c + basis.v[1]*innerRadius*s,
                    start[2] + basis.n[2]*z0 + basis.u[2]*innerRadius*c + basis.v[2]*innerRadius*s
                );
            }
        } else {
            double tc[3] = { start[0]+basis.n[0]*z0, start[1]+basis.n[1]*z0, start[2]+basis.n[2]*z0 };
            topCenterId = points->InsertNextPoint(tc);
        }

        double bc[3] = { start[0]+basis.n[0]*(z0+height), start[1]+basis.n[1]*(z0+height), start[2]+basis.n[2]*(z0+height) };
        vtkIdType bottomCenterId = points->InsertNextPoint(bc);

        auto addTri = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
            auto tri = vtkSmartPointer<vtkTriangle>::New();
            tri->GetPointIds()->SetId(0,a); tri->GetPointIds()->SetId(1,b); tri->GetPointIds()->SetId(2,c);
            polys->InsertNextCell(tri);
        };
        for (int i = 0; i < resolution; ++i) {
            vtkIdType t0 = 2*i, b0 = t0+1, t1 = 2*((i+1)%resolution), b1 = t1+1;
            addTri(t0,t1,b1); addTri(t0,b1,b0);  // 外壁
            if (innerRadius > 0.0) {
                vtkIdType i0 = innerTopIds[i], i1 = innerTopIds[(i+1)%resolution];
                addTri(i0,t0,t1); addTri(i0,t1,i1);    // 环形顶盖
            } else {
                addTri(topCenterId,t0,t1);               // 实心顶盖（反向=向外法线时 t0,t1 顺序）
            }
            addTri(bottomCenterId,b1,b0);                // 底盖
        }
        if (innerTopIds) delete[] innerTopIds;

        auto poly = vtkSmartPointer<vtkPolyData>::New();
        poly->SetPoints(points); poly->SetPolys(polys);
        return poly;
    }

    // ---- 带螺纹圆柱（支持内径）----
    vtkSmartPointer<vtkPolyData> BuildThreadedCylinderWorld(double radius, double innerRadius, double depth,
        double height, int turns, int resolution, double z0,
        const double start[3], const Basis& basis) {
        if (turns <= 0 || depth <= 0.0)
            return BuildCylinderWorld(radius, innerRadius, height, resolution, z0, start, basis);

        int resTheta = std::max(16, resolution);
        int resZ     = std::max(resolution * turns * 2, turns * 16);
        double full  = vtkMath::Pi() * 2.0;
        double pitch = height / static_cast<double>(turns);
        double flatGap = 0.25, fadeLen = 0.20;

        double needed = 2.0 * (flatGap + fadeLen);
        if (needed >= height) { double sc = height / needed; flatGap *= sc; fadeLen *= sc; }

        double fadeInStart  = flatGap;
        double fadeInEnd    = flatGap + fadeLen;
        double fadeOutStart = height - (flatGap + fadeLen);
        double fadeOutEnd   = height - flatGap;

        auto points = vtkSmartPointer<vtkPoints>::New();
        auto polys  = vtkSmartPointer<vtkCellArray>::New();

        auto radiusAt = [&](double zLocal, double theta) {
            if (zLocal < flatGap || zLocal > fadeOutEnd) return radius;
            double phase = (zLocal / pitch) - (theta / full);
            double wave  = std::sin(full * phase);
            double fade  = 1.0;
            if (zLocal < fadeInEnd)
                fade = qBound(0.0, (zLocal - fadeInStart) / fadeLen, 1.0);
            else if (zLocal > fadeOutStart)
                fade = qBound(0.0, (fadeOutEnd - zLocal) / fadeLen, 1.0);
            return radius + depth * wave * fade;
        };

        auto pointId = [&](int iz, int it) {
            return static_cast<vtkIdType>(iz * resTheta + (it % resTheta));
        };

        for (int iz = 0; iz <= resZ; ++iz) {
            double tz = static_cast<double>(iz) / resZ;
            double z  = z0 + height * tz;
            for (int it = 0; it < resTheta; ++it) {
                double theta = full * it / resTheta;
                double r = radiusAt(z - z0, theta);
                double c = std::cos(theta), s = std::sin(theta);
                points->InsertNextPoint(
                    start[0] + basis.n[0]*z + basis.u[0]*r*c + basis.v[0]*r*s,
                    start[1] + basis.n[1]*z + basis.u[1]*r*c + basis.v[1]*r*s,
                    start[2] + basis.n[2]*z + basis.u[2]*r*c + basis.v[2]*r*s
                );
            }
        }

        for (int iz = 0; iz < resZ; ++iz) {
            for (int it = 0; it < resTheta; ++it) {
                vtkIdType p00=pointId(iz,it), p01=pointId(iz,it+1);
                vtkIdType p10=pointId(iz+1,it), p11=pointId(iz+1,it+1);
                auto tri1 = vtkSmartPointer<vtkTriangle>::New();
                tri1->GetPointIds()->SetId(0,p00); tri1->GetPointIds()->SetId(1,p01); tri1->GetPointIds()->SetId(2,p11);
                polys->InsertNextCell(tri1);
                auto tri2 = vtkSmartPointer<vtkTriangle>::New();
                tri2->GetPointIds()->SetId(0,p00); tri2->GetPointIds()->SetId(1,p11); tri2->GetPointIds()->SetId(2,p10);
                polys->InsertNextCell(tri2);
            }
        }

        // 端盖（顶部支持内径环形盖）
        auto addCap = [&](bool isBottom) {
            int iz = isBottom ? resZ : 0;
            double z = z0 + height * (isBottom ? 1.0 : 0.0);

            if (!isBottom && innerRadius > 0.0) {
                // 顶部：环形盖
                vtkIdType* innerIds = new vtkIdType[resTheta];
                for (int it = 0; it < resTheta; ++it) {
                    double theta = full * it / resTheta;
                    double c = std::cos(theta), s = std::sin(theta);
                    innerIds[it] = points->InsertNextPoint(
                        start[0] + basis.n[0]*z + basis.u[0]*innerRadius*c + basis.v[0]*innerRadius*s,
                        start[1] + basis.n[1]*z + basis.u[1]*innerRadius*c + basis.v[1]*innerRadius*s,
                        start[2] + basis.n[2]*z + basis.u[2]*innerRadius*c + basis.v[2]*innerRadius*s
                    );
                }
                for (int it = 0; it < resTheta; ++it) {
                    vtkIdType p0 = pointId(iz,it), p1 = pointId(iz,it+1);
                    vtkIdType i0 = innerIds[it],    i1 = innerIds[(it+1)%resTheta];
                    auto tri1 = vtkSmartPointer<vtkTriangle>::New();
                    tri1->GetPointIds()->SetId(0,i0); tri1->GetPointIds()->SetId(1,p0); tri1->GetPointIds()->SetId(2,p1);
                    polys->InsertNextCell(tri1);
                    auto tri2 = vtkSmartPointer<vtkTriangle>::New();
                    tri2->GetPointIds()->SetId(0,i0); tri2->GetPointIds()->SetId(1,p1); tri2->GetPointIds()->SetId(2,i1);
                    polys->InsertNextCell(tri2);
                }
                delete[] innerIds;
            } else {
                double center[3] = { start[0]+basis.n[0]*z, start[1]+basis.n[1]*z, start[2]+basis.n[2]*z };
                vtkIdType centerId = points->InsertNextPoint(center);
                for (int it = 0; it < resTheta; ++it) {
                    vtkIdType p0 = pointId(iz,it), p1 = pointId(iz,it+1);
                    auto tri = vtkSmartPointer<vtkTriangle>::New();
                    if (isBottom) {
                        tri->GetPointIds()->SetId(0,centerId); tri->GetPointIds()->SetId(1,p1); tri->GetPointIds()->SetId(2,p0);
                    } else {
                        tri->GetPointIds()->SetId(0,centerId); tri->GetPointIds()->SetId(1,p0); tri->GetPointIds()->SetId(2,p1);
                    }
                    polys->InsertNextCell(tri);
                }
            }
        };
        addCap(true);
        addCap(false);

        auto poly = vtkSmartPointer<vtkPolyData>::New();
        poly->SetPoints(points); poly->SetPolys(polys);
        return poly;
    }

    // ---- 半球（冠部）----
    vtkSmartPointer<vtkPolyData> BuildHemisphereWorld(double radius, double height, int resolution,
        double z0, const double start[3], const Basis& basis) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto polys  = vtkSmartPointer<vtkCellArray>::New();
        const double full = vtkMath::Pi() * 2.0;
        int thetaRes = std::max(12, resolution * 2);
        int phiRes   = std::max(8, resolution);

        for (int ip = 0; ip <= phiRes; ++ip) {
            double phi    = (vtkMath::Pi() * 0.5) * ip / phiRes;
            double rxy    = radius * std::cos(phi);
            double zlocal = height * std::sin(phi);
            for (int it = 0; it < thetaRes; ++it) {
                double theta = full * it / thetaRes;
                double c = std::cos(theta), s = std::sin(theta);
                points->InsertNextPoint(
                    start[0] + basis.n[0]*(z0+zlocal) + basis.u[0]*rxy*c + basis.v[0]*rxy*s,
                    start[1] + basis.n[1]*(z0+zlocal) + basis.u[1]*rxy*c + basis.v[1]*rxy*s,
                    start[2] + basis.n[2]*(z0+zlocal) + basis.u[2]*rxy*c + basis.v[2]*rxy*s
                );
            }
        }
        auto addCell = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
            auto tri = vtkSmartPointer<vtkTriangle>::New();
            tri->GetPointIds()->SetId(0,a); tri->GetPointIds()->SetId(1,b); tri->GetPointIds()->SetId(2,c);
            polys->InsertNextCell(tri);
        };
        auto idx = [&](int ip, int it) { return static_cast<vtkIdType>(ip * thetaRes + it % thetaRes); };
        for (int ip = 0; ip < phiRes; ++ip)
            for (int it = 0; it < thetaRes; ++it) {
                addCell(idx(ip,it), idx(ip,it+1), idx(ip+1,it+1));
                addCell(idx(ip,it), idx(ip+1,it+1), idx(ip+1,it));
            }
        auto poly = vtkSmartPointer<vtkPolyData>::New();
        poly->SetPoints(points); poly->SetPolys(polys);
        return poly;
    }

    // ---- 内孔壁（仅侧壁 + 底盖，用于中空洞的内表面）----
    vtkSmartPointer<vtkPolyData> BuildInnerHoleWorld(double radius, double height, int resolution,
        double z0, const double start[3], const Basis& basis) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto polys  = vtkSmartPointer<vtkCellArray>::New();
        const double full = vtkMath::Pi() * 2.0;

        for (int i = 0; i < resolution; ++i) {
            double angle = full * i / resolution;
            double c = std::cos(angle), s = std::sin(angle);
            points->InsertNextPoint(
                start[0] + basis.n[0]*z0 + basis.u[0]*radius*c + basis.v[0]*radius*s,
                start[1] + basis.n[1]*z0 + basis.u[1]*radius*c + basis.v[1]*radius*s,
                start[2] + basis.n[2]*z0 + basis.u[2]*radius*c + basis.v[2]*radius*s
            );
            points->InsertNextPoint(
                start[0] + basis.n[0]*(z0+height) + basis.u[0]*radius*c + basis.v[0]*radius*s,
                start[1] + basis.n[1]*(z0+height) + basis.u[1]*radius*c + basis.v[1]*radius*s,
                start[2] + basis.n[2]*(z0+height) + basis.u[2]*radius*c + basis.v[2]*radius*s
            );
        }
        double bc[3] = { start[0]+basis.n[0]*(z0+height), start[1]+basis.n[1]*(z0+height), start[2]+basis.n[2]*(z0+height) };
        vtkIdType bottomCenterId = points->InsertNextPoint(bc);

        auto addTri = [&](vtkIdType a, vtkIdType b, vtkIdType c) {
            auto tri = vtkSmartPointer<vtkTriangle>::New();
            tri->GetPointIds()->SetId(0,a); tri->GetPointIds()->SetId(1,b); tri->GetPointIds()->SetId(2,c);
            polys->InsertNextCell(tri);
        };
        for (int i = 0; i < resolution; ++i) {
            vtkIdType t0 = 2*i, b0 = t0+1, t1 = 2*((i+1)%resolution), b1 = t1+1;
            addTri(t0, b1, t1); addTri(t0, b0, b1);    // 内壁（法线朝内）
            addTri(bottomCenterId, b0, b1);               // 孔底盖（法线朝向种植体内部）
        }
        auto poly = vtkSmartPointer<vtkPolyData>::New();
        poly->SetPoints(points); poly->SetPolys(polys);
        return poly;
    }

} // namespace

// ============================================================
// ImplantCreator 植体实现
// ============================================================

class ImplantCreator::Impl {
public:
    double startPoint[3]{ 0.0, 0.0, 0.0 };
    double totalRadius{ 1.25 };
    double innerRadius{ 0.0 };
    double neckRadius{ 1.25 };
    double neckHeight{ 4.0 };
    double bodyHeight{ 8.0 };
    double headHeight{ 1.0 };
    int    resolution{ 32 };
    double threadDepth{ 0.0 };
    int    threadTurns{ 0 };

    vtkSmartPointer<vtkActor>          actor;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
};

ImplantCreator::ImplantCreator() : pImpl(std::make_unique<Impl>()) {}
ImplantCreator::~ImplantCreator() = default;

ImplantCreator::ImplantCreator(ImplantCreator&& other) : pImpl(std::move(other.pImpl)) {
    savePath = std::move(other.savePath);
}
ImplantCreator& ImplantCreator::operator=(ImplantCreator&& other) noexcept {
    if (this != &other) { pImpl = std::move(other.pImpl); savePath = std::move(other.savePath); }
    return *this;
}

void ImplantCreator::setStartPoint(double x, double y, double z) {
    pImpl->startPoint[0] = x; pImpl->startPoint[1] = y; pImpl->startPoint[2] = z;
}
void ImplantCreator::setTotalDiameter(double diameter)  { pImpl->totalRadius  = diameter / 2.0; }
void ImplantCreator::setInnerDiameter(double diameter)  { pImpl->innerRadius  = diameter / 2.0; }
void ImplantCreator::setNeckHeight(double height)        { pImpl->neckHeight   = height; }
void ImplantCreator::setBodyHeight(double height)        { pImpl->bodyHeight   = height; }
void ImplantCreator::setHeadHeight(double height)        { pImpl->headHeight   = height; }
void ImplantCreator::setNeckDiameter(double diameter)   { pImpl->neckRadius   = diameter / 2.0; }
void ImplantCreator::setResolution(int resolution)       { pImpl->resolution   = resolution; }
void ImplantCreator::setThreadDepth(double depth)        { pImpl->threadDepth  = depth; }
void ImplantCreator::setThreadTurns(int turns)           { pImpl->threadTurns  = turns; }

bool ImplantCreator::saveActor() {
    if (!pImpl->actor || !pImpl->actor->GetMapper()) return false;
    vtkPolyData* data = vtkPolyData::SafeDownCast(pImpl->actor->GetMapper()->GetInput());
    return SavePolyDataToFile(data, savePath);
}

bool ImplantCreator::buildActor(int resolution) {
    const double radius = pImpl->totalRadius;
    if (radius <= 1e-6) return false;

    // 鲁棒性：内径不得大于等于外径（底层强制保证，与 UI 是否限制无关）
    double safeInnerRadius = pImpl->innerRadius;
    if (safeInnerRadius >= radius) {
        safeInnerRadius = radius - 1e-3;
        if (safeInnerRadius < 0.0) safeInnerRadius = 0.0;
    }

    const int segments = std::max(8, (pImpl->resolution > 3 ? pImpl->resolution : resolution));
    const double neckH = pImpl->neckHeight > 0.0 ? pImpl->neckHeight : 1.0;
    const double bodyH = pImpl->bodyHeight > 0.0 ? pImpl->bodyHeight : 2.0;
    const double headH = pImpl->headHeight > 0.0 ? pImpl->headHeight : 1.0;

    if (neckH <= 1e-6 || bodyH <= 1e-6 || headH <= 1e-6) return false;

    double dir[3] = { 0.0, 0.0, -1.0 };
    Basis basis = MakeBasis(dir);

    const double threadDepth = pImpl->threadDepth;
    const int    threadTurns = pImpl->threadTurns;

    auto body = (threadDepth > 0.0 && threadTurns > 0)
        ? BuildThreadedCylinderWorld(radius, safeInnerRadius, threadDepth, bodyH, threadTurns, segments, 0.0, pImpl->startPoint, basis)
        : BuildCylinderWorld(radius, safeInnerRadius, bodyH, segments, 0.0, pImpl->startPoint, basis);
    auto head = BuildHemisphereWorld(radius, headH, segments, bodyH, pImpl->startPoint, basis);

    auto append = vtkSmartPointer<vtkAppendPolyData>::New();
    append->AddInputData(body);
    append->AddInputData(head);
    if (safeInnerRadius > 0.0) {
        auto hole = BuildInnerHoleWorld(safeInnerRadius, bodyH, segments, 0.0, pImpl->startPoint, basis);
        append->AddInputData(hole);
    }
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

vtkActor* ImplantCreator::getActor() const { return pImpl->actor; }

// ============================================================
// BaseCreator 基台实现
// ============================================================

class BaseCreator::Impl {
public:
    double baseCenter[3]{ 0.0, 0.0, 0.0 };
    double neckHeight{ 4.0 };
    double neckRadius{ 1.25 };
    double baseBottomRadius{ 2.5 };
    double baseTopLoftRadius{ 2.5 };
    double baseAngle{ 15.0 };
    double baseAzimuth{ 0.0 };
    double baseHeight{ 5.0 };
    int    resolution{ 32 };

    vtkSmartPointer<vtkActor>          baseActor;
    vtkSmartPointer<vtkPolyDataMapper> baseMapper;
};

BaseCreator::BaseCreator() : pImpl(std::make_unique<Impl>()) {}
BaseCreator::~BaseCreator() = default;

BaseCreator::BaseCreator(BaseCreator&& other) : pImpl(std::move(other.pImpl)) {
    baseSavePath = std::move(other.baseSavePath);
}
BaseCreator& BaseCreator::operator=(BaseCreator&& other) noexcept {
    if (this != &other) { pImpl = std::move(other.pImpl); baseSavePath = std::move(other.baseSavePath); }
    return *this;
}

void BaseCreator::setBaseCenter(double x, double y, double z) {
    pImpl->baseCenter[0] = x; pImpl->baseCenter[1] = y; pImpl->baseCenter[2] = z;
}
void BaseCreator::setNeckHeight(double height)              { pImpl->neckHeight        = height; }
void BaseCreator::setNeckDiameter(double diameter)          { pImpl->neckRadius        = diameter / 2.0; }
void BaseCreator::setBaseBottomDiameter(double diameter)    { pImpl->baseBottomRadius  = diameter / 2.0; }
void BaseCreator::setBaseTopDiameter(double diameter)       { pImpl->baseTopLoftRadius = diameter / 2.0; }
void BaseCreator::setBaseAngle(double angle)                { pImpl->baseAngle         = qBound(0.0, angle, 50.0); }
void BaseCreator::setBaseAzimuth(double angle)              { pImpl->baseAzimuth       = angle; }
void BaseCreator::setBaseHeight(double height)              { pImpl->baseHeight        = height; }
void BaseCreator::setResolution(int resolution)             { pImpl->resolution        = resolution; }

bool BaseCreator::saveBase() {
    if (!pImpl->baseActor || !pImpl->baseActor->GetMapper()) return false;
    vtkPolyData* data = vtkPolyData::SafeDownCast(pImpl->baseActor->GetMapper()->GetInput());
    return SavePolyDataToFile(data, baseSavePath);
}

bool BaseCreator::buildBase(int resolution) {
    double normal[3] = { 0.0, 0.0, 1.0 };

    const double bottomRadius = pImpl->baseBottomRadius  > 1e-6 ? pImpl->baseBottomRadius  : 1.0;
    const double topRadius    = pImpl->baseTopLoftRadius > 1e-6 ? pImpl->baseTopLoftRadius : bottomRadius;
    const double height       = pImpl->baseHeight        > 1e-6 ? pImpl->baseHeight        : 1.0;
    if (bottomRadius <= 1e-6 || topRadius <= 1e-6 || height <= 1e-6) return false;

    const int    segments        = std::max(8, (pImpl->resolution > 3 ? pImpl->resolution : resolution));
    const Basis  lowerBasis      = MakeBasis(normal);
    const double angleRadians    = DegreesToRadians(pImpl->baseAngle);
    const double azimuthRadians  = DegreesToRadians(pImpl->baseAzimuth);
    const double length          = height / std::cos(angleRadians);

    double lateral[3] = {
        lowerBasis.u[0] * std::cos(azimuthRadians) + lowerBasis.v[0] * std::sin(azimuthRadians),
        lowerBasis.u[1] * std::cos(azimuthRadians) + lowerBasis.v[1] * std::sin(azimuthRadians),
        lowerBasis.u[2] * std::cos(azimuthRadians) + lowerBasis.v[2] * std::sin(azimuthRadians)
    };
    vtkMath::Normalize(lateral);

    double centerline[3] = {
        normal[0] * std::cos(angleRadians) + lateral[0] * std::sin(angleRadians),
        normal[1] * std::cos(angleRadians) + lateral[1] * std::sin(angleRadians),
        normal[2] * std::cos(angleRadians) + lateral[2] * std::sin(angleRadians)
    };
    vtkMath::Normalize(centerline);

    const double neckHeight = pImpl->neckHeight > 0.0 ? pImpl->neckHeight : 1.0;
    const double neckRadius = pImpl->neckRadius > 0.0 ? pImpl->neckRadius : 1.2;

    CircleFrame bottomFrame{};
    bottomFrame.center[0] = pImpl->baseCenter[0] + normal[0] * neckHeight;
    bottomFrame.center[1] = pImpl->baseCenter[1] + normal[1] * neckHeight;
    bottomFrame.center[2] = pImpl->baseCenter[2] + normal[2] * neckHeight;
    bottomFrame.basis  = lowerBasis;
    bottomFrame.radius = bottomRadius;

    CircleFrame topFrame{};
    topFrame.center[0] = bottomFrame.center[0] + centerline[0] * length;
    topFrame.center[1] = bottomFrame.center[1] + centerline[1] * length;
    topFrame.center[2] = bottomFrame.center[2] + centerline[2] * length;
    topFrame.basis  = lowerBasis;
    topFrame.radius = topRadius;

    auto neckLayer  = BuildCylinderWorld(neckRadius, 0.0, neckHeight, segments, 0.0, pImpl->baseCenter, lowerBasis);
    auto bottomCap  = BuildDiskWorld(bottomFrame, segments, true);
    auto topCap     = BuildDiskWorld(topFrame, segments, false);
    auto wall       = BuildLoftWallWorld(bottomFrame, topFrame, segments);

    auto append = vtkSmartPointer<vtkAppendPolyData>::New();
    append->AddInputData(neckLayer);
    append->AddInputData(bottomCap);
    append->AddInputData(topCap);
    append->AddInputData(wall);
    append->Update();

    auto clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputConnection(append->GetOutputPort());
    clean->Update();

    pImpl->baseMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    pImpl->baseMapper->SetInputConnection(clean->GetOutputPort());
    pImpl->baseActor = vtkSmartPointer<vtkActor>::New();
    pImpl->baseActor->SetMapper(pImpl->baseMapper);
    return true;
}

vtkActor* BaseCreator::getBase() const { return pImpl->baseActor; }
