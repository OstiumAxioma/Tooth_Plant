#ifndef CUSTOMIZE_IMPLANT_H
#define CUSTOMIZE_IMPLANT_H

#include <memory>
#include "data-define/DataDefine.h"

class vtkActor;


class ComponentCreator {
public:
    ComponentCreator();
    ~ComponentCreator();
    ComponentCreator(DataDefine::ImplantInfoStu);
    ComponentCreator(ComponentCreator&&);
    ComponentCreator& operator=(ComponentCreator&&) noexcept;

    void setStartPoint(double x, double y, double z);
    void setEndPoint(double x, double y, double z);

    void setNeckHeight(double height);
    void setBodyHeight(double height);
    void setHeadHeight(double height);
    void setBaseTopDiameter(double radius);
    void setResolution(int resolution);
    void setThreadDepth(double depth);
    void setThreadTurns(int turns);
    void setTotalDiameter(double radius);

    bool buildActor(int resolution = 32);
    bool saveActor();
    vtkActor* getActor() const;

    bool buildBase(int resolution = 32);
    bool saveBase();
    vtkActor* getBase() const;
    void setBaseCenter(double x, double y, double z);
    void setBaseBottomRadius(double radius);
    void setBaseTopRadius(double radius);
    void setBaseAngle(double angle);
    void setBaseAzimuth(double angle);
    void setBaseLength(double length);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    std::string savePath;
    std::string baseSavePath;
};


#endif // CUSTOMIZE_IMPLANT_H
