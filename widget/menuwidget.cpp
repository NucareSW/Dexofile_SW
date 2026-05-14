#include "widget/menuwidget.h"
#include "ui_menuwidget.h"
#include "component/navigationcomponent.h"
#include "component/componentmanager.h" // Added for ComponentManager
#include "component/inputcomponent.h"   // Added for InputComponent
#include "util/util.h"                // Added for LOG_INFO, LOG_ERROR
#include "widget/NcButton.h" // Assuming NcButton is the class for ui->button1 etc.

#include <QMouseEvent>

MenuWidget::MenuWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MenuWidget)
{
    ui->setupUi(this);

    // Connect button clicks to their respective handlers
    connect(ui->btnLeft, &NcButton::clicked, this, &MenuWidget::onLeftButtonClicked);
    connect(ui->btnCenter, &NcButton::clicked, this, &MenuWidget::onCenterButtonClicked);
    connect(ui->btnRight, &NcButton::clicked, this, &MenuWidget::onRightButtonClicked);

    connect(ui->btnLeft, &NcButton::longClicked, this, &MenuWidget::onLeftButtonLongClicked);
    connect(ui->btnCenter, &NcButton::longClicked, this, &MenuWidget::onCenterButtonLongClicked);
    connect(ui->btnRight, &NcButton::longClicked, this, &MenuWidget::onRightButtonLongClicked);

    connect(ui->btnCenter, &NcButton::cancelled, this, [this](auto) {
        // Chỉ hiện ShutdownDialog khi nút Center đang ở trạng thái "Start" (không đang đo)
        auto centerAction = getActionFor(ActionType::CENTER);
        if (!centerAction || centerAction->name == "Start") {
            navigation::toShutdownDlg();
        }
    });


    // Connect to InputComponent signals
    nucare::InputComponent* inputComp = ComponentManager::instance().inputComponent();
    if (inputComp) {
        connect(inputComp, &nucare::InputComponent::btnLeftPress, this, [this]{ simulateButtonPress(ui->btnLeft); });
        connect(inputComp, &nucare::InputComponent::btnLeftRelease, this, [this]{ simulateButtonRelease(ui->btnLeft); });
        connect(inputComp, &nucare::InputComponent::btnCenterPress, this, [this]{ simulateButtonPress(ui->btnCenter); });
        connect(inputComp, &nucare::InputComponent::btnCenterRelease, this, [this]{ simulateButtonRelease(ui->btnCenter); });
        connect(inputComp, &nucare::InputComponent::btnRightPress, this, [this]{ simulateButtonPress(ui->btnRight); });
        connect(inputComp, &nucare::InputComponent::btnRightRelease, this, [this]{ simulateButtonRelease(ui->btnRight); });
        nucare::logI() << "MenuWidget: Connected to InputComponent signals.";
    } else {
        nucare::logE() << "MenuWidget: InputComponent is null, cannot connect input signals.";
    }

    // Register as a global navigation listener
    navigation::NavigationComponent::registerGlobalListener(this);
}

MenuWidget::~MenuWidget()
{
    // Unregister as a global navigation listener
    navigation::NavigationComponent::unregisterGlobalListener(this);
    delete ui;
}

QSize MenuWidget::sizeHint() const { return ui->btnCenter->iconSize(); }

// Slots for global navigation events
void MenuWidget::onGlobalViewPushed(navigation::NavigationComponent* source, BaseView* newView)
{
    Q_UNUSED(source);
    Q_UNUSED(newView);
    updateMenu();
}

void MenuWidget::onGlobalViewPopped(navigation::NavigationComponent* source, BaseView* oldView)
{
    Q_UNUSED(source);
    Q_UNUSED(oldView);
    updateMenu();
}

navigation::NavigationComponent *MenuWidget::navigation()
{
    return ComponentManager::instance().navigationComponent();
}

void MenuWidget::updateMenu()
{
    QString leftTxt, longLeftTxt;
    QString centerTxt, longCenterTxt;
    QString rightTxt, longRightTxt;

    const char* icLeft = nullptr;
    const char* icCenter = nullptr;
    const char* icRight = nullptr;

    if (auto leftAction = getActionFor(LEFT)) {
        leftTxt = leftAction->name;
        icLeft = leftAction->icon ? leftAction->icon : nullptr;
    }
    if (auto centerAction = getActionFor(CENTER)) {
        centerTxt = centerAction->name;
        icCenter = centerAction->icon ? centerAction->icon : nullptr;
    }
    if (auto rightAction = getActionFor(RIGHT)) {
        rightTxt = rightAction->name;
        icRight = rightAction->icon ? rightAction->icon : nullptr;
    }
    if (auto longLeftAction = getActionFor(LONG_LEFT)) {
        longLeftTxt = longLeftAction->name;
        icLeft = longLeftAction->icon ? longLeftAction->icon : icLeft;
    }
    if (auto longCenterAction = getActionFor(LONG_CENTER)) {
        longCenterTxt = longCenterAction->name;
        icCenter = longCenterAction->icon ? longCenterAction->icon : icCenter;
    }
    if (auto longRightAction = getActionFor(LONG_RIGHT)) {
        longRightTxt = longRightAction->name;
        icRight = longRightAction->icon ? longRightAction->icon : icRight;
    }

    if (!longLeftTxt.isEmpty()) {
        leftTxt += "/" + longLeftTxt;
    }
    if (!longCenterTxt.isEmpty()) {
        centerTxt += "/" + longCenterTxt;
    }
    if (!longRightTxt.isEmpty()) {
        rightTxt += "/" + longRightTxt;
    }

    if (!icLeft) {
        ui->btnLeft->setText(leftTxt);
        ui->btnLeft->setIcon(QIcon());
    } else {
        ui->btnLeft->setText("");
        ui->btnLeft->setIcon(QIcon(QPixmap(icLeft)));
    }

    if (!icCenter) {
        ui->btnCenter->setText(centerTxt);
        ui->btnCenter->setIcon(QIcon());
    } else {
        ui->btnCenter->setText("");
        ui->btnCenter->setIcon(QIcon(QPixmap(icCenter)));
    }

    if (!icRight) {
        ui->btnRight->setText(rightTxt);
        ui->btnRight->setIcon(QIcon());
    } else {
        ui->btnRight->setText("");
        ui->btnRight->setIcon(QIcon(QPixmap(icRight)));
    }
}

void MenuWidget::onLeftButtonClicked()
{
    if (auto nav = navigation()) {
        if (onMenuSelected()) return;

        if (auto action = getActionFor(ActionType::LEFT)) {
            action->action();
        }
    }
}

void MenuWidget::onCenterButtonClicked()
{
    if (auto nav = navigation()) {
        if (onMenuSelected()) return;

        if (auto action = getActionFor(ActionType::CENTER)) {
            action->action();
        }
    }
}

void MenuWidget::onRightButtonClicked()
{
    if (auto nav = navigation()) {
        if (onMenuSelected()) return;

        if (auto action = getActionFor(ActionType::RIGHT)) {
            action->action();
        }
    }
}

void MenuWidget::onLeftButtonLongClicked(long)
{
    if (auto nav = navigation()) {
        if (onMenuSelected()) return;

        if (auto action = getActionFor(ActionType::LONG_LEFT)) {
            action->action();
        }
    }
}

void MenuWidget::onCenterButtonLongClicked(long)
{
    if (auto nav = navigation()) {
        if (onMenuSelected()) return;

        if (auto action = getActionFor(ActionType::LONG_CENTER)) {
            action->action();
        }
    }
}

void MenuWidget::onRightButtonLongClicked(long)
{
    if (auto nav = navigation()) {
        if (onMenuSelected()) return;

        if (auto action = getActionFor(ActionType::LONG_RIGHT)) {
            action->action();
        }
    }
}

void MenuWidget::simulateButtonPress(NcButton* button)
{
    if (!button) return;
    QPoint center = button->rect().center();
    QMouseEvent pressEvent(QEvent::MouseButtonPress, center, button->mapToGlobal(center), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(button, &pressEvent);
}

void MenuWidget::simulateButtonRelease(NcButton* button)
{
    if (!button) return;
    QPoint center = button->rect().center();
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, center, button->mapToGlobal(center), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(button, &releaseEvent);
}

std::shared_ptr<ViewAction> MenuWidget::getActionFor(BaseView* view, ActionType type) {
    switch (type) {
    case LEFT:
        return view->getLeftAction();
    case CENTER:
        return view->getCenterAction();
    case RIGHT:
        return view->getRightAction();
    case LONG_LEFT:
        return view->getLongLeftAction();
    case LONG_CENTER:
        return view->getLongCenterAction();
    case LONG_RIGHT:
        return view->getLongRightAction();
    default:
        return nullptr;
    }
}

std::shared_ptr<ViewAction> MenuWidget::getActionFor(ActionType type) {
    auto nav = navigation();
    auto curScreen = nav->curScreen(false);
    if (curScreen == nullptr) {
        return nullptr;
    }

    while (true) {
        nav = curScreen->getChildNavigation();
        if (!nav) {
            break;
        }

        auto exCurScreen = nav->curScreen(false);
        if (!exCurScreen)
            break;

        auto action = getActionFor(exCurScreen, type);
        if (!action) break;

        curScreen = exCurScreen;
    }

    return getActionFor(curScreen, type);
}

bool MenuWidget::onMenuSelected()
{
    return false;
}
