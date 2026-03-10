#ifndef CREATE_COMPONENT_H
#define CREATE_COMPONENT_H

#include <memory>
#include "data-define/DataDefine.h"

// 前向声明VTK类，避免在头文件中包含VTK头文件
class vtkActor;


class ComponentCreator {
public:
    ComponentCreator();
    ~ComponentCreator();
    ComponentCreator(DataDefine::ImplantInfoStu);
    ComponentCreator(ComponentCreator&&);
    ComponentCreator& operator=(ComponentCreator&&) noexcept;
    // 设置组件起点/终点
    void setStartPoint(double x, double y, double z);
    void setEndPoint(double x, double y, double z);

    // 尺寸配置（neck=原base截锥，body=原neck圆柱）
    void setNeckHeight(double height);    // 截锥高度
    void setBodyHeight(double height);    // 圆柱高度
    void setHeadHeight(double height);
    void setBaseTopDiameter(double radius); // Neck 上底直径
    void setResolution(int resolution);
    void setThreadDepth(double depth);
    void setThreadTurns(int turns);
    void setTotalDiameter(double radius); // 整体直径

    // 生成Actor（基于当前起点/终点）
    bool buildActor(int resolution = 32);

    // 存储当前actor
    bool saveActor();

    // 获取生成的Actor
    vtkActor* getActor() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    std::string savePath;
};


#endif // CREATE_COMPONENT_H
