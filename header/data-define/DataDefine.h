#ifndef DATA_DEFINE_H
#define DATA_DEFINE_H

#include <string>
#include <utility>

namespace DataDefine {

struct StringValue {
    std::string value;

    StringValue() = default;
    StringValue(const char* text) : value(text ? text : "") {}
    StringValue(std::string text) : value(std::move(text)) {}

    StringValue& operator=(const char* text) {
        value = text ? text : "";
        return *this;
    }

    StringValue& operator=(std::string text) {
        value = std::move(text);
        return *this;
    }

    std::string toStdString() const {
        return value;
    }
};

struct ImplantInfoStu {
    double diameter{0.0};
    double length{0.0};
    double matchingDiameter{0.0};
    StringValue stlPath;
};

} // namespace DataDefine

#endif // DATA_DEFINE_H
