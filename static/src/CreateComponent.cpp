#include "CreateComponent.h"

// VTK 头文件
#include <vtkActor.h>
#include <vtkCylinderSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

namespace CreateComponent {

class ComponentCreator::Impl {
public:
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor;
    vtkSmartPointer<vtkActor> actor;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
    vtkSmartPointer<vtkCylinderSource> cylinderSource;

    Impl() {
        renderer = vtkSmartPointer<vtkRenderer>::New();
    }
};

ComponentCreator::ComponentCreator() : pImpl(std::make_unique<Impl>()) {}
ComponentCreator::~ComponentCreator() = default;

void ComponentCreator::InitializeRenderer() {
    if (pImpl->renderer) {
        pImpl->renderer->SetBackground(0.1, 0.2, 0.4); // 默认深蓝色背景
    }
}

void ComponentCreator::SetRenderWindow(vtkRenderWindow* window) {
    if (window && pImpl->renderer) {
        pImpl->renderWindow = window;
        pImpl->renderWindow->AddRenderer(pImpl->renderer);

        // 设置交互器
        pImpl->renderWindowInteractor = pImpl->renderWindow->GetInteractor();
        if (pImpl->renderWindowInteractor) {
            pImpl->renderWindowInteractor->SetRenderWindow(pImpl->renderWindow);
            pImpl->renderWindowInteractor->Initialize();
        }
    }
}

vtkRenderer* ComponentCreator::GetRenderer() {
    return pImpl->renderer;
}

void ComponentCreator::CreateCylinder(double radius, double height, int resolution) {
    // 创建圆柱体源
    pImpl->cylinderSource = vtkSmartPointer<vtkCylinderSource>::New();
    pImpl->cylinderSource->SetRadius(radius);
    pImpl->cylinderSource->SetHeight(height);
    pImpl->cylinderSource->SetResolution(resolution);
    pImpl->cylinderSource->Update();

    // 创建映射器
    pImpl->mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    pImpl->mapper->SetInputConnection(pImpl->cylinderSource->GetOutputPort());

    // 创建演员
    pImpl->actor = vtkSmartPointer<vtkActor>::New();
    pImpl->actor->SetMapper(pImpl->mapper);

    // 添加演员到渲染器
    if (pImpl->renderer) {
        pImpl->renderer->AddActor(pImpl->actor);
    }
}

void ComponentCreator::ClearActors() {
    if (pImpl->renderer) {
        pImpl->renderer->RemoveAllViewProps(); // VTK 8.2 使用 RemoveAllViewProps
    }
}

void ComponentCreator::ResetCamera() {
    if (pImpl->renderer) {
        pImpl->renderer->ResetCamera();
    }
}

void ComponentCreator::Render() {
    if (pImpl->renderWindow) {
        pImpl->renderWindow->Render();
    }
}

void ComponentCreator::SetBackground(double r, double g, double b) {
    if (pImpl->renderer) {
        pImpl->renderer->SetBackground(r, g, b);
    }
}

} // namespace CreateComponent
