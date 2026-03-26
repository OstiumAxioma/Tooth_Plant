#ifndef CUSTOMIZE_IMPLANT_H
#define CUSTOMIZE_IMPLANT_H

#include <memory>
#include <string>

class vtkActor;

// ============================================================
// 种植体生成器
// ============================================================
class ImplantCreator {
public:
    ImplantCreator();
    ~ImplantCreator();
    ImplantCreator(ImplantCreator&&);
    ImplantCreator& operator=(ImplantCreator&&) noexcept;

    // 设置种植体轴线起点。
    void setStartPoint(double x, double y, double z);
    // 设置种植体总直径。
    void setTotalDiameter(double diameter);
    // 设置种植体内径（>0 时在种植体主体打通孔，不含 head 段）。
    void setInnerDiameter(double diameter);
    // 设置穿龈颈部高度。
    void setNeckHeight(double height);
    // 设置植体主体（带螺纹段）高度。
    void setBodyHeight(double height);
    // 设置植体冠部高度。
    void setHeadHeight(double height);
    // 设置接口直径。
    void setNeckDiameter(double diameter);
    // 设置圆周采样分段数。
    void setResolution(int resolution);
    // 设置螺纹深度（<=0 则不生成螺纹）。
    void setThreadDepth(double depth);
    // 设置螺纹圈数。
    void setThreadTurns(int turns);

    // 按当前参数构建种植体 Actor，失败返回 false。
    bool buildActor(int resolution = 32);
    // 将当前网格保存为 STL，无路径时返回 false。
    bool saveActor();
    // 获取最近一次构建的种植体 Actor（未构建则返回 nullptr）。
    vtkActor* getActor() const;

    // STL 保存路径（可选，为空则 saveActor() 返回 false）。
    std::string savePath;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================
// 基台生成器
// ============================================================
class BaseCreator {
public:
    BaseCreator();
    ~BaseCreator();
    BaseCreator(BaseCreator&&);
    BaseCreator& operator=(BaseCreator&&) noexcept;

    // 设置基台生成起始点（Neck 从此处沿法向延伸，基台在 Neck 上方生成）。
    void setBaseCenter(double x, double y, double z);
    // 设置 Neck 高度。
    void setNeckHeight(double height);
    // 设置 Neck 直径。
    void setNeckDiameter(double diameter);
    // 设置基台下端直径。
    void setBaseBottomDiameter(double diameter);
    // 设置基台上端直径。
    void setBaseTopDiameter(double diameter);
    // 设置基台夹角，单位为度，范围限制为 0 到 50。
    void setBaseAngle(double angle);
    // 设置基台方位角，单位为度。
    void setBaseAzimuth(double angle);
    // 设置基台上部高度。
    void setBaseHeight(double height);
    // 设置圆周采样分段数。
    void setResolution(int resolution);

    // 按当前参数构建基台 Actor，失败返回 false。
    bool buildBase(int resolution = 32);
    // 将当前网格保存为 STL，无路径时返回 false。
    bool saveBase();
    // 获取最近一次构建的基台 Actor（未构建则返回 nullptr）。
    vtkActor* getBase() const;

    // STL 保存路径（可选，为空则 saveBase() 返回 false）。
    std::string baseSavePath;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // CUSTOMIZE_IMPLANT_H
