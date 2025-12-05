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
#include <QSlider>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QVTKOpenGLWidget.h>
#include <cmath>

// 包含静态库头文件
#include "CreateComponent.h"

// VTK头文件
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , vtkWidget(nullptr)
    , renderContainer(nullptr)
    , renderLayout(nullptr)
    , renderPlaceholder(nullptr)
    , renderer(nullptr)
    , componentCreator(std::make_unique<CreateComponent::ComponentCreator>())
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
    statusBar()->showMessage("控制面板已就绪，等待VTK初始化", 2000);
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
    auto toRadius = [](QSlider *s) { return s->value() / 10.0; };
    auto toHeight = toCoord;

    double start[3] = { toCoord(startSliders[0]), toCoord(startSliders[1]), toCoord(startSliders[2]) };
    double end[3]   = { toCoord(endSliders[0]), toCoord(endSliders[1]), toCoord(endSliders[2]) };

    componentCreator->SetStartPoint(start[0], start[1], start[2]);
    componentCreator->SetEndPoint(end[0], end[1], end[2]);

    double baseH = toHeight(baseHeightSlider);
    double neckH = toHeight(neckHeightSlider);
    double headH = toHeight(headHeightSlider);
    double baseTopR = toRadius(baseTopRadiusSlider);
    int resolution = resolutionSlider->value();

    componentCreator->SetBaseHeight(baseH);
    componentCreator->SetNeckHeight(neckH);
    componentCreator->SetHeadHeight(headH);
    componentCreator->SetBaseTopRadius(baseTopR);
    componentCreator->SetResolution(resolution);

    if(!componentCreator->BuildActor(toRadius(radiusSlider), resolution)) {
        statusBar()->showMessage("参数非法或长度不足，无法生成组件", 2000);
        return;
    }

    if (vtkActor* actor = componentCreator->GetActor()) {
        renderer->RemoveAllViewProps();
        renderer->AddActor(actor);
        renderer->ResetCamera();
        vtkWidget->GetRenderWindow()->Render();
        statusBar()->showMessage("圆柱体已更新", 1000);
    } else {
        statusBar()->showMessage("未生成有效的Actor", 2000);
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

    // 终点
    {
        auto grp = makeGroup("结束点 (X,Y,Z)");
        makeSliderRow(grp.second, "X", -100, 100, 0, endSliders[0], endValueLabels[0]);
        makeSliderRow(grp.second, "Y", -100, 100, 0, endSliders[1], endValueLabels[1]);
        makeSliderRow(grp.second, "Z", -100, 100, 20, endSliders[2], endValueLabels[2]); // 默认Z=2.0
        layout->addWidget(grp.first);
    }

    // 半径
    {
        auto grp = makeGroup("圆柱半径");
        makeSliderRow(grp.second, "R", 1, 100, 8, radiusSlider, radiusValueLabel); // 0.1~10.0
        layout->addWidget(grp.first);
    }

    // 尺寸高度
    {
        auto grp = makeGroup("几何尺寸 (单位对应坐标缩放 0.1)");
        makeSliderRow(grp.second, "Base高度", 1, 300, 6, baseHeightSlider, baseHeightValueLabel);   // 0.1~30.0
        makeSliderRow(grp.second, "Neck高度", 1, 300, 8, neckHeightSlider, neckHeightValueLabel);
        makeSliderRow(grp.second, "Head高度", 1, 300, 6, headHeightSlider, headHeightValueLabel);
        makeSliderRow(grp.second, "Base上底半径", 1, 300, 12, baseTopRadiusSlider, baseTopRadiusValueLabel); // 0.1~30.0
        makeSliderRow(grp.second, "分段(Resolution)", 8, 120, 32, resolutionSlider, resolutionValueLabel);
        layout->addWidget(grp.first);
    }

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
    connectSlider(endSliders[0]);   connectSlider(endSliders[1]);   connectSlider(endSliders[2]);
    connectSlider(radiusSlider);
    connectSlider(baseHeightSlider);
    connectSlider(neckHeightSlider);
    connectSlider(headHeightSlider);
    connectSlider(baseTopRadiusSlider);
    connectSlider(resolutionSlider);

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
    auto toRadius = [](QSlider *s) { return s->value() / 10.0; };
    auto toHeight = toCoord;

    startValueLabels[0]->setText(QString::number(toCoord(startSliders[0]), 'f', 1));
    startValueLabels[1]->setText(QString::number(toCoord(startSliders[1]), 'f', 1));
    startValueLabels[2]->setText(QString::number(toCoord(startSliders[2]), 'f', 1));

    endValueLabels[0]->setText(QString::number(toCoord(endSliders[0]), 'f', 1));
    endValueLabels[1]->setText(QString::number(toCoord(endSliders[1]), 'f', 1));
    endValueLabels[2]->setText(QString::number(toCoord(endSliders[2]), 'f', 1));

    radiusValueLabel->setText(QString::number(toRadius(radiusSlider), 'f', 1));
    baseHeightValueLabel->setText(QString::number(toHeight(baseHeightSlider), 'f', 1));
    neckHeightValueLabel->setText(QString::number(toHeight(neckHeightSlider), 'f', 1));
    headHeightValueLabel->setText(QString::number(toHeight(headHeightSlider), 'f', 1));
    baseTopRadiusValueLabel->setText(QString::number(toRadius(baseTopRadiusSlider), 'f', 1));
    resolutionValueLabel->setText(QString::number(resolutionSlider->value()));

    double start[3] = { toCoord(startSliders[0]), toCoord(startSliders[1]), toCoord(startSliders[2]) };
    double end[3]   = { toCoord(endSliders[0]), toCoord(endSliders[1]), toCoord(endSliders[2]) };
    double dx = end[0]-start[0], dy = end[1]-start[1], dz = end[2]-start[2];
    double dirLen = std::sqrt(dx*dx + dy*dy + dz*dz);
    double sumH = toHeight(baseHeightSlider) + toHeight(neckHeightSlider) + toHeight(headHeightSlider);
    lengthInfoLabel->setText(QString("方向长度 %1 / 部件总长 %2")
        .arg(dirLen, 0, 'f', 2)
        .arg(sumH, 0, 'f', 2));
}

double MainWindow::currentLength() const
{
    auto toCoord = [](QSlider *s) { return s->value() / 10.0; };
    double start[3] = { toCoord(startSliders[0]), toCoord(startSliders[1]), toCoord(startSliders[2]) };
    double end[3]   = { toCoord(endSliders[0]), toCoord(endSliders[1]), toCoord(endSliders[2]) };
    double dx = end[0]-start[0], dy = end[1]-start[1], dz = end[2]-start[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}
