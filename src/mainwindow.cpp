#include "mainwindow.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QVTKOpenGLWidget.h>

// 包含静态库头文件
#include "CreateComponent.h"

// VTK头文件（仅用于获取渲染窗口）
#include <vtkRenderWindow.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , vtkWidget(nullptr)
    , componentCreator(std::make_unique<CreateComponent::ComponentCreator>())
{
    setWindowTitle("VTK Qt 项目");
    resize(800, 600);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    
    // 先创建一个简单的界面，确保程序能稳定运行
    setupSimpleWidget();
    
    // 延迟初始化VTK组件
    QTimer::singleShot(2000, this, &MainWindow::setupVTKWidget);
}

MainWindow::~MainWindow()
{
}

void MainWindow::createActions()
{
    // 退出动作
    exitAct = new QAction("退出(&Q)", this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip("退出应用程序");
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // 关于动作
    aboutAct = new QAction("关于(&A)", this);
    aboutAct->setStatusTip("显示应用程序的关于对话框");
    connect(aboutAct, &QAction::triggered, [this]() {
        QMessageBox::about(this, "关于 VTK Qt 项目",
                          "这是一个基于VTK和Qt的3D可视化项目。\n使用静态库进行VTK渲染。");
    });

    // 创建圆柱体动作
    cylinderAct = new QAction("创建圆柱体(&C)", this);
    cylinderAct->setStatusTip("在场景中创建一个圆柱体");
    connect(cylinderAct, &QAction::triggered, this, &MainWindow::createCylinder);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(exitAct);

    helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction(aboutAct);
}

void MainWindow::createToolBars()
{
    fileToolBar = addToolBar("文件");
    fileToolBar->addAction(cylinderAct);
    fileToolBar->addAction(exitAct);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("就绪");
}

void MainWindow::setupSimpleWidget()
{
    QLabel *label = new QLabel("VTK Qt 项目正在初始化...", this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("QLabel { font-size: 18px; color: blue; }");
    setCentralWidget(label);
    statusBar()->showMessage("简单界面创建成功", 2000);
}

void MainWindow::setupVTKWidget()
{
    try {
        statusBar()->showMessage("正在初始化VTK...", 1000);

        // 步骤1：创建QVTKOpenGLWidget
        vtkWidget = new QVTKOpenGLWidget(this);
        setCentralWidget(vtkWidget);

        // 步骤2：初始化静态库中的渲染器
        componentCreator->InitializeRenderer();
        componentCreator->SetBackground(0.1, 0.2, 0.4); // 深蓝色背景

        // 步骤3：将渲染器与QVTKOpenGLWidget连接
        vtkRenderWindow* renderWindow = vtkWidget->GetRenderWindow();
        componentCreator->SetRenderWindow(renderWindow);

        statusBar()->showMessage("VTK集成到Qt界面成功！", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("VTK初始化失败: %1").arg(e.what()));
        statusBar()->showMessage("VTK初始化失败", 3000);
    }
}

void MainWindow::createCylinder()
{
    try {
        // 使用静态库创建圆柱体
        componentCreator->CreateCylinder(1.0, 2.0, 32);
        
        // 重置相机
        componentCreator->ResetCamera();
        
        // 渲染
        componentCreator->Render();

        statusBar()->showMessage("圆柱体已创建", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("创建圆柱体失败: %1").arg(e.what()));
        statusBar()->showMessage("创建圆柱体失败", 3000);
    }
}
