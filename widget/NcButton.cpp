#include "NcButton.h"

#include <QMetaEnum>
#include <chrono>
#include <QEvent>
#include <QDateTime>
#include <QTimer>

using namespace std;

#define NC_BTN_TIMEOUT 4000
#define NC_BTN_CLICK_TIMEOUT 800

NcButton::NcButton(QWidget *parent) : QPushButton(parent)
{
    installEventFilter(this);
    mTimer = new QTimer(this);
    mTimer->setSingleShot(true);
    connect(mTimer, &QTimer::timeout, this, [this]() {
        updateState(CANCEL);
    });
}

NcButton::~NcButton() {
    removeEventFilter(this);
}

bool NcButton::eventFilter(QObject *obj, QEvent *event) {
    if (obj != this) {
        return QPushButton::eventFilter(obj, event);
    }

    if (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonDblClick)        // Reset at mouse double click, because we don't support double click. So event has to be fake as button press to support custom Long click
    {
        updateState(State::PRESSED);

        // Don't send this event to system event. We will send it later
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto now = QDateTime::currentMSecsSinceEpoch();
        auto elapsedClick = now - mMousePressInTime;

        if (state == CANCEL) {
            updateState(NONE);
            return false;
        } else if (elapsedClick >= NC_BTN_CLICK_TIMEOUT) {
            // Handle long click
            updateState(NONE);
            emit longClicked(elapsedClick);
            return false;
        } else {
            updateState(NONE);
            emit QPushButton::clicked();
            return true;
        }
    }

    return QPushButton::eventFilter(obj, event);
}

void NcButton::updateState(State state)
{
    if (this->state == state) return;

    switch (state) {
    case NONE:
        this->state = state;
        mMousePressInTime = 0;
        mTimer->stop();
        setDown(false);
        break;
    case PRESSED:
        if (this->state != NONE)
            break;

        this->state = state;
        setDown(true);

        mMousePressInTime = QDateTime::currentMSecsSinceEpoch();
        mTimer->start(NC_BTN_TIMEOUT);

        break;
    case CANCEL:
        emit cancelled(NC_BTN_TIMEOUT);
        mMousePressInTime = 0;
        this->state = state;
        setDown(false);
        break;
    }
}
