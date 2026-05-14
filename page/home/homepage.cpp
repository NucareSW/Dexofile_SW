#include "homepage.h"
#include "ui_homepage.h"  // Generated UI header
#include "component/settingmanager.h"
#include "component/SpectrumAccumulator.h"
#include "util/util.h"
#include "model/Event.h"
#include "component/databasemanager.h"
#include "component/componentmanager.h"
#include "component/ncmanager.h"
#include "model/DetectorInfo.h"
#include "model/Background.h"
#include "model/Calibration.h"
#include <limits>
using namespace navigation;
using namespace setting;

void navigation::toHome(NavigationComponent* navController, NavigationEntry* entry, const QString& tag) {
    auto pWidget = dynamic_cast<QWidget*>(entry->host);
    entry->type  = TAB_IN_WINDOW;
    entry->view  = new HomePage(pWidget);
    navController->enter(entry);
}

HomePage::HomePage(QWidget* parent) : BaseScreen(tag::HOME_TAG, parent), ui(new Ui::HomePage) {
    ui->setupUi(this);

    setLeftAction(new ViewAction(
        "Events",
        [this]() {
            navigation::toEventList(this);
            return true;
        },
        nullptr));
    setRightAction(new ViewAction(
        "Settings",
        [this]() {
            navigation::toSetting(
                getNavigation(),
                new navigation::NavigationEntry(navigation::CHILD_IN_WINDOW, nullptr, nullptr, this->parent()),
                setting::buildSettingTree());
            return true;
        },
        nullptr));
    setLongLeftAction(new ViewAction("Spectrum", [this]() {
        navigation::toSpectrumViewer(this, m_accumulator);
        return true;
    }));

    auto settingMgr = ComponentManager::instance().settingManager();
    auto builder = SpectrumAccumulator::Builder()
            .setParent(this)
            .setMode(AccumulationMode::ContinuousByTime)
            .setContinuousInterval(settingMgr->getMeasureInterval())
            .setTimeoutSeconds(settingMgr->getAcqTimeId());
    m_accumulator = std::shared_ptr<SpectrumAccumulator>(builder.build());
    connect(m_accumulator.get(), &SpectrumAccumulator::stateChanged, this, &HomePage::stateChanged);
    connect(m_accumulator.get(), &SpectrumAccumulator::accumulationUpdated, this, &HomePage::updateEvent);

    settingMgr->subscribeKey(SettingManager::KEY_ACQTIME_ID, (QObject*) this, [this](setting::SettingManager* mgr, auto) {
        setMeasureTime(mgr->getAcqTimeId());
    });
    settingMgr->subscribeKey(SettingManager::KEY_MEASURE_INVERVAL, static_cast<QObject*>(this), [this](SettingManager* mgr, auto) {
        setIntervalTime(mgr->getMeasureInterval());
    });
    settingMgr->subscribeKey(SettingManager::KEY_PIPE_MATERIAL, static_cast<QObject*>(this), [this](SettingManager* mgr, auto) {
        setPipeMaterial(mgr->getPipeMaterial());
    });
    settingMgr->subscribeKey(SettingManager::KEY_PIPE_DIAMETER, static_cast<QObject*>(this), [this](SettingManager* mgr, auto) {
        setPipeDiameter(mgr->getPipeDiameter());
    });
    settingMgr->subscribeKey(SettingManager::KEY_PIPE_THICKNESS, static_cast<QObject*>(this), [this](SettingManager* mgr, auto) {
        setPipeThickness(mgr->getPipeThickness());
    });
    settingMgr->subscribeKey(SettingManager::KEY_PIPE_DENSITY, static_cast<QObject*>(this), [this](SettingManager* mgr, auto) {
        setPipeDensity(mgr->getPipeDensity());
    });
    settingMgr->subscribeKey(SettingManager::KEY_NDT_SRC, static_cast<QObject*>(this), [this](SettingManager* mgr, auto) {
        setIsotope(mgr->getNdtSource());
    });

    stateChanged(getState());
}

HomePage::~HomePage() { delete ui; }

void HomePage::reloadLocal() { ui->retranslateUi(this); }

void HomePage::start() {
    if (m_accumulator->getCurrentState() == AccumulatorState::Idle) {
        m_accumulator->start();
    }
}

void HomePage::stop() {
    if (m_accumulator) {
        m_accumulator->stop();
    }
}

AccumulatorState HomePage::getState()
{
    if (m_accumulator) return m_accumulator->getCurrentState();
    return AccumulatorState::Idle;
}

void HomePage::setIntervalTime(int sec)
{
    if (m_accumulator) {
        m_accumulator->setIntervalTime(sec);
        ui->intervalTimeLabel->setText(QString("Int.time: %1")
                                       .arg(datetime::formatDuration(m_accumulator->getIntervalTime())));
    }
}

void HomePage::setMeasureTime(int sec)
{
    if (m_accumulator) {
        m_accumulator->setTargetTime(sec);
        ui->stopTimeValueLabel->setText(datetime::formatDate_yyyyMMdd_HHmm(m_accumulator->getCurrentResult().finishTime));
        ui->measureTimeValueLabel->setText(datetime::formatDuration(sec));
    }
}

void HomePage::setIsotope(const QString &src)
{
    ui->isotopeLabel->setText(src);
}

void HomePage::setPipeMaterial(const QString &material)
{
    ui->materialLabel->setText(QString("Material: %1").arg(material));
}

void HomePage::setPipeDiameter(double diameter)
{
    ui->diameterLabel->setText(QString("Diameter: %1mm").arg(diameter, 0, 'f', 1));
}

void HomePage::setPipeThickness(double thickness)
{
    ui->thicknessLabel->setText(QString("Thickness: %1mm").arg(thickness, 0, 'f', 1));
}

void HomePage::setPipeDensity(double density)
{
    ui->densityLabel->setText(QString("Density: %1g/cc").arg(density, 0, 'f', 1));
}

void HomePage::stateChanged(AccumulatorState state)
{
    if (state == AccumulatorState::Idle) {
        setCenterAction(new ViewAction("Start", [this]() {
            start();
            return true;
        }));
        updateMenu();
    } else {
        setCenterAction(new ViewAction("Stop", [this]() {
            stop();
            return true;
        }));
        updateMenu();
    }

    switch (state) {
    case AccumulatorState::Idle: {
        ui->stateLabel->setText("State: Idle");
        ui->startTimeLabel->hide();
        ui->startTimeValueLabel->hide();
        ui->stopTimeLabel->hide();
        ui->stopTimeValueLabel->hide();
        ui->etTitleLabel->hide();
        ui->etValueLabel->hide();

        break;
    }
    case AccumulatorState::Measuring: {
        ui->stateLabel->setText("State: Measuring");
        ui->startTimeLabel->show();
        ui->startTimeValueLabel->show();
        ui->stopTimeLabel->show();
        ui->stopTimeValueLabel->show();
        ui->etTitleLabel->show();
        ui->etValueLabel->show();
        ui->stopTimeValueLabel->setText(datetime::formatDate_yyyyMMdd_HHmm(m_accumulator->getCurrentResult().finishTime));
        ui->startTimeValueLabel->setText(datetime::formatDate_yyyyMMdd_HHmm(m_accumulator->getCurrentResult().startTime));
        break;
    }
    case AccumulatorState::Waiting: {
        ui->stateLabel->setText("State: Waiting");
        break;
    }
    case AccumulatorState::Completed: {
        ui->stateLabel->setText("State: Completed");

        auto ret = m_accumulator->getCurrentResult();

        auto dbManager = ComponentManager::instance().databaseManager();
        auto settingMgr = ComponentManager::instance().settingManager();
        auto ncMgr = ComponentManager::instance().ncManager();
        Event event;

        // Populate Event object
        event.setSoftwareVersion(QApplication::applicationVersion());
        event.setStartedTime(ret.startTime);
        event.setFinishedTime(ret.finishTime);
        event.setLiveTime(ret.startTime.secsTo(ret.finishTime));
        event.setAvgCps(ret.avgCPS);
        event.setMaxCps(ret.maxCPS);
        double safeMinCPS = (ret.minCPS == std::numeric_limits<double>::max()) ? 0.0 : ret.minCPS;
        event.setMinCps(safeMinCPS);
        event.setAvgGamma_nSv(0);
        event.setMaxGamma_nSv(0);
        event.setMinGamma_nSv(0);
        event.setAvgFillCps(0);

        // Use IDs from AccumulationResult
        event.setDetectorId(ret.detectorId);
        event.setBackgroundId(ret.backgroundId);
        event.setCalibrationId(ret.calibrationId);

        if (ret.detectorId == -1) {
            nucare::logW() << "Detector ID is -1 in accumulation result. Event might not be correctly associated.";
        }
        if (ret.backgroundId == -1) {
            nucare::logW() << "Background ID is -1 in accumulation result.";
        }
        if (ret.calibrationId == -1) {
            nucare::logW() << "Calibration ID is -1 in accumulation result.";
        }


        QString spectrumStringData;
        ClogEstimation clog;

        if (ret.spectrum) {
            spectrumStringData = ret.spectrum->toString();
            event.setRealTime(ret.spectrum->getRealTime());
            double acqTime = ret.spectrum->getAcqTime();
            if (acqTime > 0) {
                event.setAvgFillCps(ret.spectrum->getFillCps() / acqTime);
            }

            if (ncMgr && ncMgr->getCurrentDetector()) {

                nucare::logI() << "homepage: estimateClog";

                clog = ncMgr->estimateClog(ret.spectrum, ncMgr->getCurrentDetector());
            }
        }

        auto isotopeProfile = settingMgr->getIsotopeProfile();

        event.setE1Energy(isotopeProfile->threshold_energy.first);
        event.setE1Branching(isotopeProfile->threshold_branching.first);
        event.setE1Netcount(clog.netCount1);
        event.setE2Energy(isotopeProfile->threshold_energy.second);
        event.setE2Branching(isotopeProfile->threshold_branching.second);
        event.setE2Netcount(clog.netCount2);
        event.setPipeMaterial(settingMgr->getPipeMaterial());
        event.setPipeDiameter(settingMgr->getPipeDiameter());
        double pipeThickness = settingMgr->getPipeThickness();
        event.setPipeThickness(pipeThickness);
        event.setClogThickness(clog.thickness);
        double clogRatio = pipeThickness > 0 ? clog.thickness / pipeThickness : 0.0;
        event.setClogRatio(clogRatio);

        // Cập nhật UI labels với kết quả tính clog
        ui->clogThickEstValue->setText(QString("%1 mm").arg(clog.thickness, 0, 'f', 1));
        ui->clogRatioLabel->setText(QString("Clog ratio: %1%").arg(clogRatio * 100.0, 0, 'f', 0));

        // Insert event data
        if (dbManager) {
            qlonglong eventId = dbManager->insertEvent(&event);
            nucare::logI() << "Event inserted with ID:" << eventId;

            if (eventId > 0 && !spectrumStringData.isEmpty()) {
                dbManager->insertEventDetail(eventId, spectrumStringData);
            }
        } else {
            nucare::logW() << "DatabaseManager not found!";
        }

        break;
    }
    }
}

void HomePage::updateEvent()
{
    if (m_accumulator) {
        auto& ret = m_accumulator->getCurrentResult();

        auto etTime = ret.finishTime.toSecsSinceEpoch() - QDateTime::currentSecsSinceEpoch();
        ui->etValueLabel->show();
        ui->etValueLabel->setText(datetime::formatDuration(etTime));
    }
}
