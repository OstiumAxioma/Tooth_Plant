#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// VTK模块初始化 - 解决 "no override found for 'vtkRenderer'" 错误
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2)
VTK_MODULE_INIT(vtkInteractionStyle)

#include <QMainWindow>
#include <memory>
#include <vtkSmartPointer.h>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QStatusBar;
class QToolBar;
class QLabel;
class QSlider;
class QVBoxLayout;
QT_END_NAMESPACE

class QVTKOpenGLWidget;
class vtkRenderer;
template <class T> class vtkSmartPointer;

// 前向声明
namespace CreateComponent {
    class ComponentCreator;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void setupSimpleWidget();
    void setupVTKWidget();
    void updateValueLabels();
    QWidget* buildControls();
    QWidget* buildRenderArea();
    void updateActorFromControls();

private:
    // UI组件
    QVTKOpenGLWidget *vtkWidget;
    QWidget *renderContainer;
    QVBoxLayout *renderLayout;
    QLabel *renderPlaceholder;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QAction *exitAct;
    QAction *aboutAct;

    // 渲染器和组件生成器
    vtkSmartPointer<vtkRenderer> renderer;
    std::unique_ptr<CreateComponent::ComponentCreator> componentCreator;

    // 控制滑块
    QSlider *startSliders[3];
    QSlider *endSliders[3];
    QSlider *radiusSlider;
    QLabel *startValueLabels[3];
    QLabel *endValueLabels[3];
    QLabel *radiusValueLabel;
};

#endif // MAINWINDOW_H
