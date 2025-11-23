# VTK Qt 静态库项目模板

这是一个基于VTK 8.2.0和Qt 5.12.9的3D可视化项目模板，采用静态库架构设计。

## 项目特点

- **静态库架构**: VTK渲染功能封装在独立的静态库中
- **模块化设计**: 主程序通过静态库接口调用渲染功能
- **易于扩展**: 可以方便地添加新的渲染功能到静态库

## 项目结构

```
vtk_static_template/
├── CMakeLists.txt          # 主项目CMake配置
├── build.bat              # 主程序构建脚本
├── README.md              # 项目说明
├── header/                # 主程序头文件
│   ├── mainwindow.h       # 主窗口头文件
│   └── CreateComponent.h  # 静态库接口转发
├── src/                   # 主程序源文件
│   ├── main.cpp          # 程序入口
│   ├── mainwindow.cpp    # 主窗口实现
│   └── mainwindow.ui     # Qt Designer UI文件
├── lib/                   # 静态库输出目录
│   ├── CreateComponent.lib   # Release版静态库
│   └── CreateComponent_d.lib # Debug版静态库
├── static/                # 静态库项目
│   ├── CMakeLists.txt    # 静态库CMake配置
│   ├── build.bat         # 静态库构建脚本
│   ├── header/           # 静态库头文件
│   │   └── CreateComponent.h    # VTK组件接口
│   └── src/              # 静态库源文件
│       └── CreateComponent.cpp  # VTK组件实现（生成圆柱体）
└── generate_vscode_config.py  # VSCode配置生成器
```

## 环境要求

- **VTK**: 8.2.0 (路径: `D:/code/vtk8.2.0/VTK-8.2.0`)
- **Qt**: 5.12.9 (路径: `C:/Qt/Qt5.12.9/5.12.9/msvc2017_64`)
- **编译器**: Visual Studio 2022
- **CMake**: 3.14 或更高版本

## 构建步骤

### 1. 编译静态库

```bash
cd static
build.bat
# 选择构建配置：
# 1 - Debug
# 2 - Release  
# 3 - Both (推荐)
```

静态库将被自动复制到 `lib/` 目录。

### 2. 编译主程序

```bash
# 返回根目录
cd ..
build.bat
```

可执行文件位置: `build/Exe/Release/VTK_Qt_Project.exe`

## 静态库架构说明

### CreateComponent::ComponentCreator 类

静态库提供了 `ComponentCreator` 类，封装了VTK组件生成（当前示例为圆柱体）：

```cpp
class ComponentCreator {
public:
    void InitializeRenderer();
    void SetRenderWindow(vtkRenderWindow* window);
    vtkRenderer* GetRenderer();

    // 创建圆柱体
    void CreateCylinder(double radius = 1.0, double height = 2.0, int resolution = 20);

    void ClearActors();
    void ResetCamera();
    void Render();
    void SetBackground(double r, double g, double b);
};
```

### 使用示例

```cpp
#include "CreateComponent.h"

// 创建组件生成器
auto creator = std::make_unique<CreateComponent::ComponentCreator>();

// 初始化
creator->InitializeRenderer();
creator->SetBackground(0.1, 0.2, 0.4);

// 设置渲染窗口
creator->SetRenderWindow(vtkWidget->GetRenderWindow());

// 创建3D对象（示例：圆柱体）
creator->CreateCylinder(1.0, 2.0, 32);
creator->ResetCamera();
creator->Render();
```

## 功能特性

- **3D可视化**: 基于VTK的3D图形渲染
- **Qt界面**: 现代化的Qt用户界面
- **静态库架构**: 渲染功能独立封装
- **Pimpl模式**: 隐藏VTK实现细节
- **交互式操作**: 支持鼠标交互和3D场景操作

## 默认功能

程序启动后会显示一个3D圆柱体，支持以下交互：

### 鼠标操作
- **左键拖拽**: 旋转视角
- **中键拖拽**: 平移视角
- **滚轮**: 缩放

### 菜单操作
- **文件 → 退出**: 关闭程序
- **帮助 → 关于**: 显示程序信息

### 工具栏操作
- **创建圆柱体**: 在场景中创建新圆柱体

## 扩展开发

### 在静态库中添加新功能

1. 在 `static/header/CreateComponent.h` 中添加新方法声明
2. 在 `static/src/CreateComponent.cpp` 中实现新方法
3. 重新编译静态库

示例：添加创建立方体功能

```cpp
// CreateComponent.h
void CreateCustomComponent(...);

// CreateComponent.cpp
void ComponentCreator::CreateCustomComponent(...) {
    // ... 创建数据源、mapper和actor
}
```

### 在主程序中使用新功能

```cpp
// mainwindow.cpp
void MainWindow::createCube() {
    componentCreator->CreateCustomComponent(...);
    componentCreator->ResetCamera();
    componentCreator->Render();
}
```

## 故障排除

### 编译错误

1. **找不到CreateComponent.lib**
   - 确保先编译静态库 (`cd static && build.bat`)
   - 检查 `lib/` 目录是否存在静态库文件

2. **VTK相关错误**
   - 确认VTK_DIR路径正确: `D:/code/vtk8.2.0/VTK-8.2.0`
   - VTK版本必须是8.2.0

3. **Qt相关错误**
   - 确认Qt路径正确: `C:/Qt/Qt5.12.9/5.12.9/msvc2017_64`
   - Qt版本必须是5.12.9

### VSCode IntelliSense错误

如果VSCode显示波浪线错误但编译成功：

1. 运行 `python generate_vscode_config.py` 生成配置
2. 或安装CMake Tools扩展
3. 重启VSCode

## 开发建议

1. **修改静态库后**: 必须重新编译静态库和主程序
2. **添加新的VTK模块**: 在静态库的CMakeLists.txt中配置
3. **版本控制**: `.gitignore`已配置忽略构建目录

## 许可证

本项目仅供学习和研究使用。
