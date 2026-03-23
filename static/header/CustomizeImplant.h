#ifndef CUSTOMIZE_IMPLANT_H
#define CUSTOMIZE_IMPLANT_H

#include <memory>
#include <string>
#include "data-define/DataDefine.h"

class vtkActor;

class ComponentCreator {
public:
    // 创建一个空的种植体/基台构建器。
    ComponentCreator();
    // 释放内部 VTK 资源。
    ~ComponentCreator();
    // 使用种植体信息初始化构建器和默认保存路径。
    ComponentCreator(DataDefine::ImplantInfoStu);
    // 移动构造。
    ComponentCreator(ComponentCreator&&);
    // 移动赋值。
    ComponentCreator& operator=(ComponentCreator&&) noexcept;

    // 设置种植体轴线起点。
    void setStartPoint(double x, double y, double z);
    // 设置种植体轴线终点。
    void setEndPoint(double x, double y, double z);

    // 设置颈部高度。
    void setNeckHeight(double height);
    // 设置主体高度。
    void setBodyHeight(double height);
    // 设置头部高度。
    void setHeadHeight(double height);
    // 设置颈部(Neck)直径。
    void setNeckDiameter(double diameter);
    // 设置圆周采样分段数。
    void setResolution(int resolution);
    // 设置螺纹深度。
    void setThreadDepth(double depth);
    // 设置螺纹圈数。
    void setThreadTurns(int turns);
    // 设置种植体总直径。
    void setTotalDiameter(double radius);

    // 按当前参数构建种植体 Actor。
    bool buildActor(int resolution = 32);
    // 将当前种植体网格保存为 STL。
    bool saveActor();
    // 获取最近一次构建的种植体 Actor。
    vtkActor* getActor() const;

    // 按当前参数构建基台 Actor。
    bool buildBase(int resolution = 32);
    // 将当前基台网格保存为 STL。
    bool saveBase();
    // 获取最近一次构建的基台 Actor。
    vtkActor* getBase() const;
    // 设置Neck生成起始点（Neck延法向向上延伸，基台在Neck上方生成）。
    void setBaseCenter(double x, double y, double z);
    // 设置基台下圆半径。
    void setBaseBottomRadius(double radius);
    // 设置基台上圆半径。
    void setBaseTopRadius(double radius);
    // 设置基台夹角，单位为度，范围限制为 0 到 50。
    void setBaseAngle(double angle);
    // 设置基台方位角，单位为度。
    void setBaseAzimuth(double angle);
    // 设置基台中心连线长度。
    void setBaseLength(double length);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    std::string savePath;
    std::string baseSavePath;
};

#endif // CUSTOMIZE_IMPLANT_H
