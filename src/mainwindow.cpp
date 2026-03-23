#include "mainwindow.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QSlider>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QVTKOpenGLWidget.h>
#include <cmath>

// 包含静态库测试侧声明
#include "CustomizeImplant.h"

// VTK头文件
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkProperty.h>

namespace {

QString projectRootPath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 4; ++i) {
        if (QFileInfo(dir.filePath("CMakeLists.txt")).exists()) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QCoreApplication::applicationDirPath();
}

QString stlOutputPath()
{
    return QDir(projectRootPath()).filePath("customize_implant_preview.stl");
}

QString baseStlOutputPath()
{
    return QDir(projectRootPath()).filePath("customize_implant_preview_base.stl");
}

DataDefine::ImplantInfoStu defaultImplantInfo()
{
    DataDefine::ImplantInfoStu info;
    info.diameter = 1.6;
    info.length = 4.0;
    info.matchingDiameter = 2.4;
    info.stlPath = stlOutputPath().toStdString();
    return info;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , vtkWidget(nullptr)
    , renderContainer(nullptr)
    , renderLayout(nullptr)
    , renderPlaceholder(nullptr)
    , renderer(nullptr)
    , componentCreator(std::make_unique<ComponentCreator>(defaultImplantInfo()))
{
    setWindowTitle("VTK Qt 项目");
    resize(800, 600);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    
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
    fileToolBar->addAction(exitAct);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("就绪");
}

void MainWindow::setupSimpleWidget()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(8);

    QWidget *controls = buildControls();
    renderContainer = buildRenderArea();

    mainLayout->addWidget(controls, 0);
    mainLayout->addWidget(renderContainer, 1);

    setCentralWidget(central);
    statusBar()->showMessage(
        QString("控制面板已就绪，植体/STL输出到 %1，基台输出到 %2")
            .arg(QDir::toNativeSeparators(stlOutputPath()))
            .arg(QDir::toNativeSeparators(baseStlOutputPath())),
        4000);
}

void MainWindow::setupVTKWidget()
{
    try {
        statusBar()->showMessage("正在初始化VTK...", 1000);

        // 步骤1：创建QVTKOpenGLWidget并放入渲染区域
        vtkWidget = new QVTKOpenGLWidget(this);
        if (renderPlaceholder) {
            renderLayout->removeWidget(renderPlaceholder);
            renderPlaceholder->deleteLater();
            renderPlaceholder = nullptr;
        }
        renderLayout->addWidget(vtkWidget, 1);

        // 步骤2：创建并配置渲染器
        renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->SetBackground(0.1, 0.2, 0.4); // 深蓝色背景

        // 步骤3：将渲染器与QVTKOpenGLWidget连接
        vtkRenderWindow* renderWindow = vtkWidget->GetRenderWindow();
        renderWindow->AddRenderer(renderer);

        statusBar()->showMessage("VTK集成到Qt界面成功！", 2000);

        // 初始化一次Actor展示
        updateActorFromControls();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("VTK初始化失败: %1").arg(e.what()));
        statusBar()->showMessage("VTK初始化失败", 3000);
    }
}

void MainWindow::updateActorFromControls()
{
    if(!renderer || !vtkWidget) {
        return;
    }

    auto toCoord = [](QSlider *s) { return s->value() / 10.0; };
    auto toSize = [](QSlider *s) { return s->value() / 10.0; };
    auto toHeight = toCoord;

    double start[3] = { toCoord(startSliders[0]), toCoord(startSliders[1]), toCoord(startSliders[2]) };

    componentCreator->setStartPoint(start[0], start[1], start[2]);

    double neckH = toHeight(neckHeightSlider);
    double bodyH = toHeight(bodyHeightSlider);
    double headH = toHeight(headHeightSlider);
    double totalDiameter = toSize(radiusSlider);
    double neckDiameter = toSize(neckDiameterSlider);
    int resolution = resolutionSlider->value();
    double threadDepth = toSize(threadDepthSlider);
    int threadTurns = threadTurnsSlider->value();
    double baseBottomRadius = toSize(abutmentBottomRadiusSlider);
    double baseTopRadius = toSize(abutmentTopRadiusSlider);
    double baseAngle = abutmentAngleSlider->value();
    double baseAzimuth = abutmentAzimuthSlider->value();
    double baseLength = toSize(abutmentLengthSlider);

    componentCreator->setNeckHeight(neckH);
    componentCreator->setBodyHeight(bodyH);
    componentCreator->setHeadHeight(headH);
    componentCreator->setNeckDiameter(neckDiameter);
    componentCreator->setResolution(resolution);
    componentCreator->setThreadDepth(threadDepth);
    componentCreator->setThreadTurns(threadTurns);
    componentCreator->setTotalDiameter(totalDiameter);
    componentCreator->setBaseCenter(start[0], start[1], start[2]);
    componentCreator->setBaseBottomRadius(baseBottomRadius);
    componentCreator->setBaseTopRadius(baseTopRadius);
    componentCreator->setBaseAngle(baseAngle);
    componentCreator->setBaseAzimuth(baseAzimuth);
    componentCreator->setBaseLength(baseLength);

    const bool implantOk = componentCreator->buildActor(resolution);
    const bool baseOk = componentCreator->buildBase(resolution);

    renderer->RemoveAllViewProps();

    if (implantOk) {
        if (vtkActor* actor = componentCreator->getActor()) {
            actor->GetProperty()->SetColor(0.82, 0.82, 0.85);
            renderer->AddActor(actor);
        }
    }

    if (baseOk) {
        if (vtkActor* actor = componentCreator->getBase()) {
            actor->GetProperty()->SetColor(0.92, 0.72, 0.32);
            renderer->AddActor(actor);
        }
    }

    if (!implantOk && !baseOk) {
        statusBar()->showMessage("植体和基台参数非法，无法生成模型", 2000);
        vtkWidget->GetRenderWindow()->Render();
        return;
    }

    renderer->ResetCamera();
    vtkWidget->GetRenderWindow()->Render();

    if (implantOk && baseOk) {
        statusBar()->showMessage("植体与基台模型已更新", 1200);
    } else if (implantOk) {
        statusBar()->showMessage("植体已更新，基台参数非法", 1500);
    } else {
        statusBar()->showMessage("基台已更新，植体参数非法", 1500);
    }
}

QWidget* MainWindow::buildControls()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setSpacing(8);

    auto makeGroup = [&](const QString &title) {
        auto *group = new QGroupBox(title, panel);
        auto *form = new QFormLayout(group);
        form->setLabelAlignment(Qt::AlignLeft);
        group->setLayout(form);
        return std::pair<QGroupBox*, QFormLayout*>(group, form);
    };

    auto makeSliderRow = [&](QFormLayout *form, const QString &label, int min, int max, int value, QSlider *&slider, QLabel *&valueLabel) {
        slider = new QSlider(Qt::Horizontal, panel);
        slider->setRange(min, max);
        slider->setValue(value);
        slider->setSingleStep(1);
        slider->setPageStep(5);
        valueLabel = new QLabel(panel);
        valueLabel->setFixedWidth(60);
        auto *rowWidget = new QWidget(panel);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(valueLabel, 0);
        form->addRow(label, rowWidget);
    };

    // 起点
    {
        auto grp = makeGroup("起始点 (X,Y,Z)");
        makeSliderRow(grp.second, "X", -100, 100, 0, startSliders[0], startValueLabels[0]);
        makeSliderRow(grp.second, "Y", -100, 100, 0, startSliders[1], startValueLabels[1]);
        makeSliderRow(grp.second, "Z", -100, 100, 0, startSliders[2], startValueLabels[2]);
        layout->addWidget(grp.first);
    }

    // 总直径
    {
        auto grp = makeGroup("植体总直径");
        makeSliderRow(grp.second, "D", 1, 100, 16, radiusSlider, radiusValueLabel); // 0.1~10.0
        layout->addWidget(grp.first);
    }

    // 尺寸高度
    {
        auto grp = makeGroup("几何尺寸 (单位对应坐标缩放 0.1)");
        makeSliderRow(grp.second, "Neck高度", 1, 300, 10, neckHeightSlider, neckHeightValueLabel);   // 截锥，默认1.0
        makeSliderRow(grp.second, "Body高度", 1, 300, 20, bodyHeightSlider, bodyHeightValueLabel);  // 圆柱，默认2.0
        makeSliderRow(grp.second, "Head高度", 1, 300, 10, headHeightSlider, headHeightValueLabel);  // 默认1.0
        makeSliderRow(grp.second, "Neck直径", 1, 300, 24, neckDiameterSlider, neckDiameterValueLabel); // 0.1~30.0
        makeSliderRow(grp.second, "分段(Resolution)", 8, 120, 32, resolutionSlider, resolutionValueLabel);
        makeSliderRow(grp.second, "螺纹深度", 0, 100, 1, threadDepthSlider, threadDepthValueLabel); // 默认0.1
        makeSliderRow(grp.second, "螺纹圈数", 0, 50, 20, threadTurnsSlider, threadTurnsValueLabel); // 默认20
        layout->addWidget(grp.first);
    }

    // 基台参数
    {
        auto grp = makeGroup("基台圆台参数");
        makeSliderRow(grp.second, "下圆半径", 1, 100, 12, abutmentBottomRadiusSlider, abutmentBottomRadiusValueLabel);
        makeSliderRow(grp.second, "上圆半径", 1, 100, 8, abutmentTopRadiusSlider, abutmentTopRadiusValueLabel);
        makeSliderRow(grp.second, "夹角", 0, 50, 15, abutmentAngleSlider, abutmentAngleValueLabel);
        makeSliderRow(grp.second, "方位角", 0, 359, 0, abutmentAzimuthSlider, abutmentAzimuthValueLabel);
        makeSliderRow(grp.second, "中心线长度", 1, 200, 12, abutmentLengthSlider, abutmentLengthValueLabel);
        layout->addWidget(grp.first);
    }

    abutmentCenterInfoLabel = new QLabel(panel);
    abutmentCenterInfoLabel->setStyleSheet("color: #555;");
    layout->addWidget(abutmentCenterInfoLabel);

    // 长度提示
    lengthInfoLabel = new QLabel(panel);
    lengthInfoLabel->setStyleSheet("color: #555;");
    layout->addWidget(lengthInfoLabel);

    layout->addStretch(1);
    updateValueLabels();

    // 联动更新显示 + 实时更新Actor
    auto connectSlider = [this](QSlider *s) {
        connect(s, &QSlider::valueChanged, this, &MainWindow::updateValueLabels);
        connect(s, &QSlider::valueChanged, this, &MainWindow::updateActorFromControls);
    };
    connectSlider(startSliders[0]); connectSlider(startSliders[1]); connectSlider(startSliders[2]);
    connectSlider(radiusSlider);
    connectSlider(neckHeightSlider);
    connectSlider(bodyHeightSlider);
    connectSlider(headHeightSlider);
    connectSlider(neckDiameterSlider);
    connectSlider(resolutionSlider);
    connectSlider(threadDepthSlider);
    connectSlider(threadTurnsSlider);
    connectSlider(abutmentBottomRadiusSlider);
    connectSlider(abutmentTopRadiusSlider);
    connectSlider(abutmentAngleSlider);
    connectSlider(abutmentAzimuthSlider);
    connectSlider(abutmentLengthSlider);

    return panel;
}

QWidget* MainWindow::buildRenderArea()
{
    renderContainer = new QWidget(this);
    renderLayout = new QVBoxLayout(renderContainer);
    renderLayout->setContentsMargins(0, 0, 0, 0);
    renderPlaceholder = new QLabel("VTK Qt 项目正在初始化...", renderContainer);
    renderPlaceholder->setAlignment(Qt::AlignCenter);
    renderPlaceholder->setStyleSheet("QLabel { font-size: 18px; color: blue; }");
    renderLayout->addWidget(renderPlaceholder, 1);
    return renderContainer;
}

void MainWindow::updateValueLabels()
{
    auto toCoord = [](QSlider *s) { return s->value() / 10.0; };
    auto toSize = [](QSlider *s) { return s->value() / 10.0; };
    auto toHeight = toCoord;

    startValueLabels[0]->setText(QString::number(toCoord(startSliders[0]), 'f', 1));
    startValueLabels[1]->setText(QString::number(toCoord(startSliders[1]), 'f', 1));
    startValueLabels[2]->setText(QString::number(toCoord(startSliders[2]), 'f', 1));

    radiusValueLabel->setText(QString::number(toSize(radiusSlider), 'f', 1));
    neckHeightValueLabel->setText(QString::number(toHeight(neckHeightSlider), 'f', 1));
    bodyHeightValueLabel->setText(QString::number(toHeight(bodyHeightSlider), 'f', 1));
    headHeightValueLabel->setText(QString::number(toHeight(headHeightSlider), 'f', 1));
    neckDiameterValueLabel->setText(QString::number(toSize(neckDiameterSlider), 'f', 1));
    resolutionValueLabel->setText(QString::number(resolutionSlider->value()));
    threadDepthValueLabel->setText(QString::number(toSize(threadDepthSlider), 'f', 1));
    threadTurnsValueLabel->setText(QString::number(threadTurnsSlider->value()));
    abutmentBottomRadiusValueLabel->setText(QString::number(toSize(abutmentBottomRadiusSlider), 'f', 1));
    abutmentTopRadiusValueLabel->setText(QString::number(toSize(abutmentTopRadiusSlider), 'f', 1));
    abutmentAngleValueLabel->setText(QString::number(abutmentAngleSlider->value()) + QChar(176));
    abutmentAzimuthValueLabel->setText(QString::number(abutmentAzimuthSlider->value()) + QChar(176));
    abutmentLengthValueLabel->setText(QString::number(toSize(abutmentLengthSlider), 'f', 1));

    double start[3] = { toCoord(startSliders[0]), toCoord(startSliders[1]), toCoord(startSliders[2]) };
    double sumH = toHeight(neckHeightSlider) + toHeight(bodyHeightSlider) + toHeight(headHeightSlider);
    abutmentCenterInfoLabel->setText(QString("基台下圆中心跟随植体顶部中心: (%1, %2, %3)")
        .arg(start[0], 0, 'f', 1)
        .arg(start[1], 0, 'f', 1)
        .arg(start[2], 0, 'f', 1));
    lengthInfoLabel->setText(QString("部件总长 %1")
        .arg(sumH, 0, 'f', 2));
}

double MainWindow::currentLength() const
{
    auto toHeight = [](QSlider *s) { return s->value() / 10.0; };
    return toHeight(neckHeightSlider) + toHeight(bodyHeightSlider) + toHeight(headHeightSlider);
}
