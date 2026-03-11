#include "CustomizeImplant.h"

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
#include <QtGlobal>
#include <vtkSTLWriter.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>
#include <vtkTransformPolyDataFilter.h>

    class ComponentCreator::Impl {
    public:
        double startPoint[3]{ 0.0, 0.0, 0.0 };
        double endPoint[3]{ 0.0, 0.0, 1.0 };
        double neckHeight{ -1.0 };
        double bodyHeight{ -1.0 };
        double headHeight{ -1.0 };
        double baseTopRadius{ -1.0 };
        double totalRadius{ -1.0 };
        int resolution{ 32 };
        double threadDepth{ 0.0 };
        int threadTurns{ 0 };
        double baseCenter[3]{ 0.0, 0.0, 0.0 };
        double baseBottomRadius{ -1.0 };
        double baseTopLoftRadius{ -1.0 };
        double baseAngle{ 0.0 };
        double baseAzimuth{ 0.0 };
        double baseLength{ -1.0 };

        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkActor> baseActor;
        vtkSmartPointer<vtkPolyDataMapper> baseMapper;
    };

    ComponentCreator::ComponentCreator(ComponentCreator&& other): pImpl(std::move(other.pImpl))
    {
        savePath = other.savePath;
        baseSavePath = other.baseSavePath;
    }

    ComponentCreator& ComponentCreator::operator=(ComponentCreator&& other) noexcept
    {
        if (this != &other)
        {
            this->pImpl = std::move(other.pImpl);
            savePath = other.savePath;
            baseSavePath = other.baseSavePath;
        }
        return *this;
    }

    ComponentCreator::ComponentCreator() : pImpl(std::make_unique<Impl>()) {}
    ComponentCreator::~ComponentCreator() = default;

    ComponentCreator::ComponentCreator(DataDefine::ImplantInfoStu implantInfo) : pImpl(std::make_unique<Impl>())
    {
        pImpl->totalRadius = implantInfo.diameter / 2;
        pImpl->neckHeight = 1;
        pImpl->headHeight = 1;
        pImpl->bodyHeight = (implantInfo.length - 2) <= 0 ? 2 : implantInfo.length - 2;
        pImpl->baseTopRadius = implantInfo.matchingDiameter / 2;
        pImpl->baseCenter[0] = pImpl->startPoint[0];
        pImpl->baseCenter[1] = pImpl->startPoint[1];
        pImpl->baseCenter[2] = pImpl->startPoint[2];
        pImpl->baseBottomRadius = pImpl->baseTopRadius > 0.0 ? pImpl->baseTopRadius : 1.0;
        pImpl->baseTopLoftRadius = pImpl->totalRadius > 0.0 ? pImpl->totalRadius : 0.8;
        pImpl->baseLength = 1.0;
        savePath = implantInfo.stlPath.toStdString();
        if (!savePath.empty()) {
            const std::size_t extPos = savePath.find_last_of('.');
            const std::size_t slashPos = savePath.find_last_of("/\\");
            if (extPos != std::string::npos && (slashPos == std::string::npos || extPos > slashPos)) {
                baseSavePath = savePath.substr(0, extPos) + "_base" + savePath.substr(extPos);
            } else {
                baseSavePath = savePath + "_base.stl";
            }
        }
    }

    void ComponentCreator::setStartPoint(double x, double y, double z) {
        pImpl->startPoint[0] = x;
        pImpl->startPoint[1] = y;
        pImpl->startPoint[2] = z;
    }

    void ComponentCreator::setEndPoint(double x, double y, double z) {
        pImpl->endPoint[0] = x;
        pImpl->endPoint[1] = y;
        pImpl->endPoint[2] = z;
    }

    void ComponentCreator::setTotalDiameter(double diameter)
    {
        pImpl->totalRadius = diameter / 2;
    }

    void ComponentCreator::setNeckHeight(double height) { pImpl->neckHeight = height; }
    void ComponentCreator::setBodyHeight(double height) { pImpl->bodyHeight = height; }
    void ComponentCreator::setHeadHeight(double height) { pImpl->headHeight = height; }
    void ComponentCreator::setBaseTopDiameter(double radius) { pImpl->baseTopRadius = radius / 2; }
    void ComponentCreator::setResolution(int resolution) { pImpl->resolution = resolution; }
    void ComponentCreator::setThreadDepth(double depth) { pImpl->threadDepth = depth; }
    void ComponentCreator::setThreadTurns(int turns) { pImpl->threadTurns = turns; }
    void ComponentCreator::setBaseCenter(double x, double y, double z) {
        pImpl->baseCenter[0] = x;
        pImpl->baseCenter[1] = y;
        pImpl->baseCenter[2] = z;
    }
    void ComponentCreator::setBaseBottomRadius(double radius) { pImpl->baseBottomRadius = radius; }
    void ComponentCreator::setBaseTopRadius(double radius) { pImpl->baseTopLoftRadius = radius; }
    void ComponentCreator::setBaseAngle(double angle) { pImpl->baseAngle = angle; }
    void ComponentCreator::setBaseAzimuth(double angle) { pImpl->baseAzimuth = angle; }
    void ComponentCreator::setBaseLength(double length) { pImpl->baseLength = length; }

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
            b.u[0] = refProj[0];
            b.u[1] = refProj[1];
            b.u[2] = refProj[2];
            vtkMath::Cross(b.n, b.u, b.v);
            vtkMath::Normalize(b.v);
            return b;
        }

        struct CircleFrame {
            double center[3];
            Basis basis;
            double radius{ 1.0 };
        };

        double DegreesToRadians(double angle) {
            return angle * vtkMath::Pi() / 180.0;
        }

        vtkSmartPointer<vtkPolyData> BuildDiskWorld(const CircleFrame& frame, int resolution, bool reverseWinding) {
            auto points = vtkSmartPointer<vtkPoints>::New();
            auto polys = vtkSmartPointer<vtkCellArray>::New();
            const double full = vtkMath::Pi() * 2.0;

            for (int i = 0; i < resolution; ++i) {
                const double angle = full * i / resolution;
                const double c = std::cos(angle);
                const double s = std::sin(angle);
                const double x = frame.center[0] + frame.basis.u[0] * frame.radius * c + frame.basis.v[0] * frame.radius * s;
                const double y = frame.center[1] + frame.basis.u[1] * frame.radius * c + frame.basis.v[1] * frame.radius * s;
                const double z = frame.center[2] + frame.basis.u[2] * frame.radius * c + frame.basis.v[2] * frame.radius * s;
                points->InsertNextPoint(x, y, z);
            }

            const vtkIdType centerId = points->InsertNextPoint(frame.center);
            for (int i = 0; i < resolution; ++i) {
                const vtkIdType p0 = i;
                const vtkIdType p1 = (i + 1) % resolution;
                auto tri = vtkSmartPointer<vtkTriangle>::New();
                tri->GetPointIds()->SetId(0, centerId);
                if (reverseWinding) {
                    tri->GetPointIds()->SetId(1, p1);
                    tri->GetPointIds()->SetId(2, p0);
                } else {
                    tri->GetPointIds()->SetId(1, p0);
                    tri->GetPointIds()->SetId(2, p1);
                }
                polys->InsertNextCell(tri);
            }

            auto poly = vtkSmartPointer<vtkPolyData>::New();
            poly->SetPoints(points);
            poly->SetPolys(polys);
            return poly;
        }

        vtkSmartPointer<vtkPolyData> BuildLoftWallWorld(const CircleFrame& bottom, const CircleFrame& top, int resolution) {
            auto points = vtkSmartPointer<vtkPoints>::New();
            auto polys = vtkSmartPointer<vtkCellArray>::New();
            const double full = vtkMath::Pi() * 2.0;

            for (int i = 0; i < resolution; ++i) {
                const double angle = full * i / resolution;
                const double c = std::cos(angle);
                const double s = std::sin(angle);

                const double bottomPoint[3] = {
                    bottom.center[0] + bottom.basis.u[0] * bottom.radius * c + bottom.basis.v[0] * bottom.radius * s,
                    bottom.center[1] + bottom.basis.u[1] * bottom.radius * c + bottom.basis.v[1] * bottom.radius * s,
                    bottom.center[2] + bottom.basis.u[2] * bottom.radius * c + bottom.basis.v[2] * bottom.radius * s
                };
                const double topPoint[3] = {
                    top.center[0] + top.basis.u[0] * top.radius * c + top.basis.v[0] * top.radius * s,
                    top.center[1] + top.basis.u[1] * top.radius * c + top.basis.v[1] * top.radius * s,
                    top.center[2] + top.basis.u[2] * top.radius * c + top.basis.v[2] * top.radius * s
                };

                points->InsertNextPoint(bottomPoint);
                points->InsertNextPoint(topPoint);
            }

            for (int i = 0; i < resolution; ++i) {
                const vtkIdType b0 = 2 * i;
                const vtkIdType t0 = b0 + 1;
                const vtkIdType b1 = 2 * ((i + 1) % resolution);
                const vtkIdType t1 = b1 + 1;

                auto tri1 = vtkSmartPointer<vtkTriangle>::New();
                tri1->GetPointIds()->SetId(0, b0);
                tri1->GetPointIds()->SetId(1, b1);
                tri1->GetPointIds()->SetId(2, t1);
                polys->InsertNextCell(tri1);

                auto tri2 = vtkSmartPointer<vtkTriangle>::New();
                tri2->GetPointIds()->SetId(0, b0);
                tri2->GetPointIds()->SetId(1, t1);
                tri2->GetPointIds()->SetId(2, t0);
                polys->InsertNextCell(tri2);
            }

            auto poly = vtkSmartPointer<vtkPolyData>::New();
            poly->SetPoints(points);
            poly->SetPolys(polys);
            return poly;
        }

        bool SavePolyDataToFile(vtkPolyData* polyData, const std::string& path) {
            if (!polyData || path.empty()) {
                return false;
            }

            double bounds[6];
            polyData->GetBounds(bounds);
            const double oldCenter[3] = {
                (bounds[0] + bounds[1]) / 2.0,
                (bounds[2] + bounds[3]) / 2.0,
                (bounds[4] + bounds[5]) / 2.0
            };
            const double translate[3] = { -oldCenter[0], -oldCenter[1], -oldCenter[2] };

            auto transform = vtkSmartPointer<vtkTransform>::New();
            transform->Translate(translate);

            auto transformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
            transformFilter->SetInputData(polyData);
            transformFilter->SetTransform(transform);
            transformFilter->Update();

            auto stlWriter = vtkSmartPointer<vtkSTLWriter>::New();
            stlWriter->SetFileName(path.c_str());
            stlWriter->SetInputConnection(transformFilter->GetOutputPort());
            return stlWriter->Write() == 1;
        }

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
            double flatGap = 0.25;
            double fadeLen = 0.2;

            double needed = 2.0 * (flatGap + fadeLen);
            if (needed >= height) {
                double scale = height / needed;
                flatGap *= scale;
                fadeLen *= scale;
            }

            double fadeInStart = flatGap;
            double fadeInEnd = flatGap + fadeLen;
            double fadeOutStart = height - (flatGap + fadeLen);
            double fadeOutEnd = height - flatGap;

            auto points = vtkSmartPointer<vtkPoints>::New();
            auto polys = vtkSmartPointer<vtkCellArray>::New();

            auto radiusAt = [&](double zLocal, double theta) {
                if (zLocal < flatGap || zLocal > fadeOutEnd) {
                    return radius;
                }

                double phase = (zLocal / pitch) - (theta / full);
                double wave = std::sin(full * phase);

                double fade = 1.0;
                if (zLocal < fadeInEnd) {
                    fade = qBound(0.0, (zLocal - fadeInStart) / fadeLen, 1.0);
                }
                else if (zLocal > fadeOutStart) {
                    fade = qBound(0.0, (fadeOutEnd - zLocal) / fadeLen, 1.0);
                }

                return radius + depth * wave * fade;
            };

            auto pointId = [&](int iz, int it) {
                int idx = iz * resTheta + (it % resTheta);
                return static_cast<vtkIdType>(idx);
            };

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

            auto addCap = [&](bool top) {
                int iz = top ? resZ : 0;
                double z = z0 + height * (top ? 1.0 : 0.0);
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
                    }
                    else {
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
                double zlocal = height * std::sin(phi);
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

        vtkSmartPointer<vtkPolyData> BuildThreadWorld(double radius, double depth, double height, int turns, int resolution,
            double z0, const double start[3], const Basis& basis) {
            auto empty = vtkSmartPointer<vtkPolyData>::New();
            return empty;
        }

    } // namespace

    bool ComponentCreator::saveActor()
    {
        if (!pImpl->actor || !pImpl->actor->GetMapper()) {
            return false;
        }
        vtkPolyData* originalData = vtkPolyData::SafeDownCast(pImpl->actor->GetMapper()->GetInput());
        return SavePolyDataToFile(originalData, savePath);
    }

    bool ComponentCreator::buildActor(int resolution) {
        auto radius = pImpl->totalRadius;
        double dir[3] = {
            pImpl->startPoint[0] - pImpl->endPoint[0],
            pImpl->startPoint[1] - pImpl->endPoint[1],
            pImpl->startPoint[2] - pImpl->endPoint[2]
        };

        double length = vtkMath::Norm(dir);
        if (length <= 1e-6 || radius <= 1e-6) {
            return false;
        }

        vtkMath::Normalize(dir);

        int segments = std::max(8, (pImpl->resolution > 3 ? pImpl->resolution : resolution));

        double neckH = pImpl->neckHeight > 0.0 ? pImpl->neckHeight : 1.0;
        double bodyH = pImpl->bodyHeight > 0.0 ? pImpl->bodyHeight : 2.0;
        double headH = pImpl->headHeight > 0.0 ? pImpl->headHeight : 1.0;

        if (neckH <= 1e-6 || bodyH <= 1e-6 || headH <= 1e-6) {
            return false;
        }

        double topRadius = pImpl->baseTopRadius > radius ? pImpl->baseTopRadius : radius * 1.1;
        if (topRadius <= radius) {
            topRadius = radius * 1.05;
        }

        Basis basis = MakeBasis(dir);

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

    vtkActor* ComponentCreator::getActor() const { return pImpl->actor; }

    bool ComponentCreator::saveBase()
    {
        if (!pImpl->baseActor || !pImpl->baseActor->GetMapper()) {
            return false;
        }
        vtkPolyData* baseData = vtkPolyData::SafeDownCast(pImpl->baseActor->GetMapper()->GetInput());
        return SavePolyDataToFile(baseData, baseSavePath);
    }

    bool ComponentCreator::buildBase(int resolution) {
        double normal[3] = {
            pImpl->endPoint[0] - pImpl->startPoint[0],
            pImpl->endPoint[1] - pImpl->startPoint[1],
            pImpl->endPoint[2] - pImpl->startPoint[2]
        };
        if (vtkMath::Norm(normal) <= 1e-6) {
            normal[0] = 0.0;
            normal[1] = 0.0;
            normal[2] = 1.0;
        } else {
            vtkMath::Normalize(normal);
        }

        const double bottomRadius = pImpl->baseBottomRadius > 1e-6 ? pImpl->baseBottomRadius : 1.0;
        const double topRadius = pImpl->baseTopLoftRadius > 1e-6 ? pImpl->baseTopLoftRadius : bottomRadius;
        const double length = pImpl->baseLength > 1e-6 ? pImpl->baseLength : 1.0;
        if (bottomRadius <= 1e-6 || topRadius <= 1e-6 || length <= 1e-6) {
            return false;
        }

        const int segments = std::max(8, (pImpl->resolution > 3 ? pImpl->resolution : resolution));
        const Basis lowerBasis = MakeBasis(normal);
        const double angleRadians = DegreesToRadians(pImpl->baseAngle);
        const double azimuthRadians = DegreesToRadians(pImpl->baseAzimuth);

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

        CircleFrame bottomFrame{};
        bottomFrame.center[0] = pImpl->baseCenter[0];
        bottomFrame.center[1] = pImpl->baseCenter[1];
        bottomFrame.center[2] = pImpl->baseCenter[2];
        bottomFrame.basis = lowerBasis;
        bottomFrame.radius = bottomRadius;

        CircleFrame topFrame{};
        topFrame.center[0] = bottomFrame.center[0] + centerline[0] * length;
        topFrame.center[1] = bottomFrame.center[1] + centerline[1] * length;
        topFrame.center[2] = bottomFrame.center[2] + centerline[2] * length;
        topFrame.basis = MakeBasisWithReference(centerline, lateral);
        topFrame.radius = topRadius;

        auto bottomCap = BuildDiskWorld(bottomFrame, segments, true);
        auto topCap = BuildDiskWorld(topFrame, segments, false);
        auto wall = BuildLoftWallWorld(bottomFrame, topFrame, segments);

        auto append = vtkSmartPointer<vtkAppendPolyData>::New();
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

    vtkActor* ComponentCreator::getBase() const { return pImpl->baseActor; }
