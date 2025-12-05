#ifndef CREATE_COMPONENT_H
#define CREATE_COMPONENT_H

#include <memory>

// 前向声明VTK类，避免在头文件中包含VTK头文件
class vtkActor;

namespace CreateComponent {

class ComponentCreator {
public:
    ComponentCreator();
    ~ComponentCreator();

    // 设置组件起点/终点
    void SetStartPoint(double x, double y, double z);
    void SetEndPoint(double x, double y, double z);

    // 尺寸配置
    void SetBaseHeight(double height);
    void SetNeckHeight(double height);
    void SetHeadHeight(double height);
    void SetBaseTopRadius(double radius);
    void SetResolution(int resolution);
    void SetThreadDepth(double depth);
    void SetThreadTurns(int turns);

    // 生成Actor（基于当前起点/终点）
    bool BuildActor(double radius = 1.0, int resolution = 32);

    // 获取生成的Actor
    vtkActor* GetActor() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace CreateComponent

#endif // CREATE_COMPONENT_H
