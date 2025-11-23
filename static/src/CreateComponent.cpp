#include "CreateComponent.h"

// VTK 头文件
#include <vtkActor.h>
#include <vtkCylinderSource.h>
#include <vtkMath.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

namespace CreateComponent {

class ComponentCreator::Impl {
public:
    double startPoint[3]{0.0, 0.0, 0.0};
    double endPoint[3]{0.0, 0.0, 1.0};
    vtkSmartPointer<vtkActor> actor;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
    vtkSmartPointer<vtkCylinderSource> cylinderSource;
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

bool ComponentCreator::BuildActor(double radius, int resolution) {
    double dir[3] = {
        pImpl->endPoint[0] - pImpl->startPoint[0],
        pImpl->endPoint[1] - pImpl->startPoint[1],
        pImpl->endPoint[2] - pImpl->startPoint[2]
    };

    double length = vtkMath::Norm(dir);
    if (length <= 1e-6) {
        return false; // 起点和终点重合，无法生成
    }

    vtkMath::Normalize(dir);

    // Cylinder along Z axis, height = length
    pImpl->cylinderSource = vtkSmartPointer<vtkCylinderSource>::New();
    pImpl->cylinderSource->SetRadius(radius);
    pImpl->cylinderSource->SetHeight(length);
    pImpl->cylinderSource->SetResolution(resolution);
    pImpl->cylinderSource->SetCenter(0.0, 0.0, 0.0);
    pImpl->cylinderSource->SetCapping(1);
    pImpl->cylinderSource->Update();

    // 计算从Z轴到目标方向的旋转
    double zAxis[3] = {0.0, 0.0, 1.0};
    double rotationAxis[3];
    vtkMath::Cross(zAxis, dir, rotationAxis);
    double axisNorm = vtkMath::Norm(rotationAxis);

    double angleDeg = vtkMath::DegreesFromRadians(std::acos(vtkMath::Dot(zAxis, dir)));
    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->Identity();

    if (axisNorm > 1e-6) {
        vtkMath::Normalize(rotationAxis);
        transform->RotateWXYZ(angleDeg, rotationAxis);
    } else if (angleDeg > 179.9) {
        // 反向时选择任意正交轴旋转180度
        transform->RotateWXYZ(180.0, 1.0, 0.0, 0.0);
    }

    // 平移到中心点
    double midPoint[3] = {
        (pImpl->startPoint[0] + pImpl->endPoint[0]) * 0.5,
        (pImpl->startPoint[1] + pImpl->endPoint[1]) * 0.5,
        (pImpl->startPoint[2] + pImpl->endPoint[2]) * 0.5
    };
    transform->Translate(midPoint);

    auto transformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
    transformFilter->SetInputConnection(pImpl->cylinderSource->GetOutputPort());
    transformFilter->SetTransform(transform);
    transformFilter->Update();

    pImpl->mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    pImpl->mapper->SetInputConnection(transformFilter->GetOutputPort());

    pImpl->actor = vtkSmartPointer<vtkActor>::New();
    pImpl->actor->SetMapper(pImpl->mapper);

    return true;
}

vtkActor* ComponentCreator::GetActor() const {
    return pImpl->actor;
}

} // namespace CreateComponent
