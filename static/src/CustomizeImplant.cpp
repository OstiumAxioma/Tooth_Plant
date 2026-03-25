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
        double neckHeight{ 4.0 };
        double bodyHeight{ 8.0 };
        double headHeight{ 1.0 };
        double neckRadius{ 1.25 };
        double totalRadius{ 1.25 };
        double innerRadius{ 0.0 };
        int resolution{ 32 };
        double threadDepth{ 0.0 };
        int threadTurns{ 0 };
        double baseCenter[3]{ 0.0, 0.0, 0.0 };
        double baseBottomRadius{ 2.5 };
        double baseTopLoftRadius{ 2.5 };
        double baseAngle{ 15.0 };
        double baseAzimuth{ 0.0 };
        double baseHeight{ 5.0 };

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
        if (implantInfo.diameter > 0.0) pImpl->totalRadius = implantInfo.diameter / 2.0;
        if (implantInfo.matchingDiameter > 0.0) pImpl->neckRadius = implantInfo.matchingDiameter / 2.0;
        
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

    void ComponentCreator::setTotalDiameter(double diameter)
    {
        pImpl->totalRadius = diameter / 2;
    }

    void ComponentCreator::setInnerDiameter(double diameter)
    {
        pImpl->innerRadius = diameter / 2.0;
    }

    void ComponentCreator::setNeckHeight(double height) { pImpl->neckHeight = height; }
    void ComponentCreator::setBodyHeight(double height) { pImpl->bodyHeight = height; }
    void ComponentCreator::setHeadHeight(double height) { pImpl->headHeight = height; }
    void ComponentCreator::setNeckDiameter(double diameter) { pImpl->neckRadius = diameter / 2.0; }
    void ComponentCreator::setResolution(int resolution) { pImpl->resolution = resolution; }
    void ComponentCreator::setThreadDepth(double depth) { pImpl->threadDepth = depth; }
    void ComponentCreator::setThreadTurns(int turns) { pImpl->threadTurns = turns; }
    void ComponentCreator::setBaseCenter(double x, double y, double z) {
        pImpl->baseCenter[0] = x;
        pImpl->baseCenter[1] = y;
        pImpl->baseCenter[2] = z;
    }
    void ComponentCreator::setBaseBottomDiameter(double diameter) { pImpl->baseBottomRadius = diameter / 2.0; }
    void ComponentCreator::setBaseTopDiameter(double diameter) { pImpl->baseTopLoftRadius = diameter / 2.0; }
    void ComponentCreator::setBaseAngle(double angle) { pImpl->baseAngle = qBound(0.0, angle, 50.0); }
    void ComponentCreator::setBaseAzimuth(double angle) { pImpl->baseAzimuth = angle; }
    void ComponentCreator::setBaseHeight(double height) { pImpl->baseHeight = height; }

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

        Basis MakeBasisFromPrevious(const double dir[3], const Basis& previous) {
            double projectedU[3] = {
                previous.u[0] - vtkMath::Dot(previous.u, dir) * dir[0],
                previous.u[1] - vtkMath::Dot(previous.u, dir) * dir[1],
                previous.u[2] - vtkMath::Dot(previous.u, dir) * dir[2]
            };
            if (vtkMath::Norm(projectedU) > 1e-6) {
                return MakeBasisWithReference(dir, previous.u);
            }

            double projectedV[3] = {
                previous.v[0] - vtkMath::Dot(previous.v, dir) * dir[0],
                previous.v[1] - vtkMath::Dot(previous.v, dir) * dir[1],
                previous.v[2] - vtkMath::Dot(previous.v, dir) * dir[2]
            };
            if (vtkMath::Norm(projectedV) > 1e-6) {
                return MakeBasisWithReference(dir, previous.v);
            }

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

        vtkSmartPointer<vtkPolyData> BuildCylinderWorld(double radius, double innerRadius, double height, int resolution, double z0,
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

            vtkIdType topCenterId = -1;
            vtkIdType* innerIds = nullptr;
            if (innerRadius > 0.0) {
                innerIds = new vtkIdType[resolution];
                for (int i = 0; i < resolution; ++i) {
                    double angle = full * i / resolution;
                    double r = innerRadius;
                    double c = std::cos(angle), s = std::sin(angle);
                    double x = start[0] + basis.n[0] * z0 + basis.u[0] * r * c + basis.v[0] * r * s;
                    double y = start[1] + basis.n[1] * z0 + basis.u[1] * r * c + basis.v[1] * r * s;
                    double zc = start[2] + basis.n[2] * z0 + basis.u[2] * r * c + basis.v[2] * r * s;
                    innerIds[i] = points->InsertNextPoint(x, y, zc);
                }
            } else {
                double topCenter[3] = { start[0] + basis.n[0] * z0,
                                        start[1] + basis.n[1] * z0,
                                        start[2] + basis.n[2] * z0 };
                topCenterId = points->InsertNextPoint(topCenter);
            }

            double bottomCenter[3] = { start[0] + basis.n[0] * (z0 + height),
                                       start[1] + basis.n[1] * (z0 + height),
                                       start[2] + basis.n[2] * (z0 + height) };
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
                if (innerRadius > 0.0) {
                    vtkIdType i0 = innerIds[i];
                    vtkIdType i1 = innerIds[(i + 1) % resolution];
                    addTri(i0, t0, t1);
                    addTri(i0, t1, i1);
                } else {
                    addTri(topCenterId, t0, t1);
                }
                addTri(bottomCenterId, b1, b0);
            }
            if (innerIds) delete[] innerIds;

            auto poly = vtkSmartPointer<vtkPolyData>::New();
            poly->SetPoints(points);
            poly->SetPolys(polys);
            return poly;
        }

        vtkSmartPointer<vtkPolyData> BuildThreadedCylinderWorld(double radius, double innerRadius, double depth, double height, int turns,
            int resolution, double z0,
            const double start[3], const Basis& basis) {
            if (turns <= 0 || depth <= 0.0) {
                return BuildCylinderWorld(radius, innerRadius, height, resolution, z0, start, basis);
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

            auto addCap = [&](bool isBottom) {
                int iz = isBottom ? resZ : 0;
                double z = z0 + height * (isBottom ? 1.0 : 0.0);
                
                if (!isBottom && innerRadius > 0.0) {
                    vtkIdType* innerIds = new vtkIdType[resTheta];
                    for (int it = 0; it < resTheta; ++it) {
                        double theta = full * it / resTheta;
                        double r = innerRadius;
                        double c = std::cos(theta), s = std::sin(theta);
                        double x = start[0] + basis.n[0] * z + basis.u[0] * r * c + basis.v[0] * r * s;
                        double y = start[1] + basis.n[1] * z + basis.u[1] * r * c + basis.v[1] * r * s;
                        double zc = start[2] + basis.n[2] * z + basis.u[2] * r * c + basis.v[2] * r * s;
                        innerIds[it] = points->InsertNextPoint(x, y, zc);
                    }
                    for (int it = 0; it < resTheta; ++it) {
                        vtkIdType p0 = pointId(iz, it);
                        vtkIdType p1 = pointId(iz, it + 1);
                        vtkIdType i0 = innerIds[it];
                        vtkIdType i1 = innerIds[(it + 1) % resTheta];
                        
                        auto tri1 = vtkSmartPointer<vtkTriangle>::New();
                        tri1->GetPointIds()->SetId(0, i0);
                        tri1->GetPointIds()->SetId(1, p0);
                        tri1->GetPointIds()->SetId(2, p1);
                        polys->InsertNextCell(tri1);
                        
                        auto tri2 = vtkSmartPointer<vtkTriangle>::New();
                        tri2->GetPointIds()->SetId(0, i0);
                        tri2->GetPointIds()->SetId(1, p1);
                        tri2->GetPointIds()->SetId(2, i1);
                        polys->InsertNextCell(tri2);
                    }
                    delete[] innerIds;
                } else {
                    double center[3] = { start[0] + basis.n[0] * z,
                                         start[1] + basis.n[1] * z,
                                         start[2] + basis.n[2] * z };
                    vtkIdType centerId = points->InsertNextPoint(center);
                    for (int it = 0; it < resTheta; ++it) {
                        vtkIdType p0 = pointId(iz, it);
                        vtkIdType p1 = pointId(iz, it + 1);
                        auto tri = vtkSmartPointer<vtkTriangle>::New();
                        if (isBottom) {
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

        vtkSmartPointer<vtkPolyData> BuildInnerHoleWorld(double radius, double height, int resolution, double z0,
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

            double bottomCenter[3] = { start[0] + basis.n[0] * (z0 + height),
                                       start[1] + basis.n[1] * (z0 + height),
                                       start[2] + basis.n[2] * (z0 + height) };
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

                // Inner wall surface faces INWARD
                addTri(t0, b1, t1);
                addTri(t0, b0, b1);

                // Bottom Cap normal faces UPwards (into the hole)
                addTri(bottomCenterId, b0, b1);
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
        double dir[3] = { 0.0, 0.0, -1.0 };

        if (radius <= 1e-6) {
            return false;
        }

        double safeInnerRadius = pImpl->innerRadius;
        if (safeInnerRadius >= radius) {
            safeInnerRadius = radius - 1e-3;
            if (safeInnerRadius < 0.0) safeInnerRadius = 0.0;
        }

        int segments = std::max(8, (pImpl->resolution > 3 ? pImpl->resolution : resolution));

        double neckH = pImpl->neckHeight > 0.0 ? pImpl->neckHeight : 1.0;
        double bodyH = pImpl->bodyHeight > 0.0 ? pImpl->bodyHeight : 2.0;
        double headH = pImpl->headHeight > 0.0 ? pImpl->headHeight : 1.0;

        if (neckH <= 1e-6 || bodyH <= 1e-6 || headH <= 1e-6) {
            return false;
        }

        Basis basis = MakeBasis(dir);

        double threadDepth = pImpl->threadDepth > 0.0 ? pImpl->threadDepth : 0.1;
        int threadTurns = pImpl->threadTurns > 0 ? pImpl->threadTurns : 20;

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
        double normal[3] = { 0.0, 0.0, 1.0 };

        const double bottomRadius = pImpl->baseBottomRadius > 1e-6 ? pImpl->baseBottomRadius : 1.0;
        const double topRadius = pImpl->baseTopLoftRadius > 1e-6 ? pImpl->baseTopLoftRadius : bottomRadius;
        const double height = pImpl->baseHeight > 1e-6 ? pImpl->baseHeight : 1.0;
        if (bottomRadius <= 1e-6 || topRadius <= 1e-6 || height <= 1e-6) {
            return false;
        }

        const int segments = std::max(8, (pImpl->resolution > 3 ? pImpl->resolution : resolution));
        const Basis lowerBasis = MakeBasis(normal);
        const double angleRadians = DegreesToRadians(pImpl->baseAngle);
        const double azimuthRadians = DegreesToRadians(pImpl->baseAzimuth);
        const double length = height / std::cos(angleRadians);

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

        double neckHeight = pImpl->neckHeight > 0.0 ? pImpl->neckHeight : 1.0;
        double neckRadius = pImpl->neckRadius > 0.0 ? pImpl->neckRadius : 1.2;

        CircleFrame bottomFrame{};
        bottomFrame.center[0] = pImpl->baseCenter[0] + normal[0] * neckHeight;
        bottomFrame.center[1] = pImpl->baseCenter[1] + normal[1] * neckHeight;
        bottomFrame.center[2] = pImpl->baseCenter[2] + normal[2] * neckHeight;
        bottomFrame.basis = lowerBasis;
        bottomFrame.radius = bottomRadius;

        CircleFrame topFrame{};
        topFrame.center[0] = bottomFrame.center[0] + centerline[0] * length;
        topFrame.center[1] = bottomFrame.center[1] + centerline[1] * length;
        topFrame.center[2] = bottomFrame.center[2] + centerline[2] * length;
        topFrame.basis = bottomFrame.basis;
        topFrame.radius = topRadius;

        auto neckLayer = BuildCylinderWorld(neckRadius, 0.0, neckHeight, segments, 0.0, pImpl->baseCenter, lowerBasis);

        auto bottomCap = BuildDiskWorld(bottomFrame, segments, true);
        auto topCap = BuildDiskWorld(topFrame, segments, false);
        auto wall = BuildLoftWallWorld(bottomFrame, topFrame, segments);

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

    vtkActor* ComponentCreator::getBase() const { return pImpl->baseActor; }
