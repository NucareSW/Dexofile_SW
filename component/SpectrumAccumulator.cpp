#include "SpectrumAccumulator.h"
#include "component/componentmanager.h" // For DetectorComponent, NcManager
#include "component/ncmanager.h"        // For NcManager signals
#include "model/DetectorProp.h"       // For dev->properties()->getOriginSpc()
#include "model/Spectrum.h"           // For Spectrum_t and Spectrum aliases (includes HwSpectrum)
#include "util/util.h"                // For nucare::logX
#include "util/nc_exception.h"        // For NC_THROW_X
#include "model/DetectorInfo.h"
#include "model/Background.h"
#include "model/Calibration.h"

SpectrumAccumulator::SpectrumAccumulator(const Builder& builder, QObject* parent)
    : QObject(parent),
      m_mode(builder.m_mode),
      m_currentState(AccumulatorState::Idle),
      m_targetCountValue(builder.m_targetCountValue),
      m_timeoutValueSeconds(builder.m_timeoutValueSeconds),
      m_continuousIntervalSeconds(builder.m_continuousIntervalSeconds),
      m_continuousIntervalTimer(new QTimer(this)),
      m_accumulationTimer(new QTimer(this)) {

    if (m_mode == AccumulationMode::ByTime || m_mode == AccumulationMode::ContinuousByTime) {
        m_activeAccumulationType = ActiveSpectrumType::TypeSpectrum;
        m_curResult.spectrum = std::make_shared<Spectrum>();
        m_curResult.hwSpectrum = nullptr;
    } else if (m_mode == AccumulationMode::ByCount || m_mode == AccumulationMode::ContinuousByCount) {
        m_activeAccumulationType = ActiveSpectrumType::TypeHwSpectrum;
        m_curResult.hwSpectrum = std::make_shared<HwSpectrum>();
        m_curResult.spectrum = nullptr;
    } else {
        m_activeAccumulationType = ActiveSpectrumType::None;
        m_curResult.spectrum = nullptr;
        m_curResult.hwSpectrum = nullptr;
        nucare::logE() << "SpectrumAccumulator: Mode not set at construction or invalid mode! Mode:" << static_cast<int>(m_mode);
    }
    m_curResult.activeType = m_activeAccumulationType;
    m_continuousIntervalTimer->setSingleShot(true);

    connect(m_accumulationTimer, &QTimer::timeout, this, &SpectrumAccumulator::onAccumulationTimeout);
    connect(m_continuousIntervalTimer, &QTimer::timeout, this, &SpectrumAccumulator::onContinuousIntervalTimeout);

    NcManager* ncMgr = ComponentManager::instance().ncManager().get();
    if (ncMgr) {
        connect(ncMgr, &NcManager::spectrumReceived, this, &SpectrumAccumulator::onNcManagerSpectrumReceived);
    } else {
        nucare::logE() << "SpectrumAccumulator: NcManager instance is null. SpectrumAccumulator will not receive spectrum data.";
    }
    nucare::logI() << "SpectrumAccumulator: Instance created. Mode: " << static_cast<int>(m_mode) << ", ActiveType: " << static_cast<int>(m_activeAccumulationType);
}

SpectrumAccumulator::~SpectrumAccumulator() {
    nucare::logI() << "SpectrumAccumulator: Shutting down...";
    stop(); // Ensure timers are stopped and state is finalized (likely Idle)

    NcManager* ncMgr = ComponentManager::instance().ncManager().get();
    if (ncMgr) {
        // Disconnect all signals from ncMgr to this object
        bool disconnected = QObject::disconnect(ncMgr, 0, this, 0);
        if(disconnected) {
            nucare::logI() << "SpectrumAccumulator: Disconnected signals from NcManager.";
        } else {
            nucare::logW() << "SpectrumAccumulator: Failed to disconnect signals from NcManager, or no signals were connected.";
        }
    }
    nucare::logI() << "SpectrumAccumulator: Destroyed.";
}

AccumulationResult& SpectrumAccumulator::getCurrentResult() {
    return m_curResult;
}
AccumulatorState SpectrumAccumulator::getCurrentState() const {
    return m_currentState;
}
AccumulationMode SpectrumAccumulator::getCurrentMode() const {
    return m_mode;
}

int SpectrumAccumulator::getAcqTime() const
{
    return m_timeoutValueSeconds;
}

int SpectrumAccumulator::getTargetCount() const
{
    return m_targetCountValue;
}

int SpectrumAccumulator::getIntervalTime() const
{
    return m_continuousIntervalSeconds;
}

void SpectrumAccumulator::transitionToState(AccumulatorState newState) {
    if (m_currentState == newState) {
        return;
    }
    AccumulatorState oldState = m_currentState;
    m_currentState = newState;
    nucare::logI() << "SpectrumAccumulator: State changed from " << static_cast<int>(oldState) << " to " << static_cast<int>(m_currentState);
    emit stateChanged(m_currentState);
}

void SpectrumAccumulator::start() {
    nucare::logI() << "SpectrumAccumulator: Start requested. Current state: " << static_cast<int>(m_currentState);
    if (m_mode == AccumulationMode::NotSet || m_activeAccumulationType == ActiveSpectrumType::None) {
        nucare::logW() << "SpectrumAccumulator: Mode not set or active spectrum type is None. Configure via Builder first. Mode:" << static_cast<int>(m_mode) << ", ActiveType: " << static_cast<int>(m_activeAccumulationType);
        return;
    }

    if (m_currentState == AccumulatorState::Idle) {
        transitionToState(AccumulatorState::Waiting);
        internalStartAccumulation();
    } else {
        nucare::logW() << "SpectrumAccumulator: Cannot start from current state " << static_cast<int>(m_currentState) << ". Must be Idle.";
    }
}

void SpectrumAccumulator::stop() {
    nucare::logI() << "SpectrumAccumulator: Manual stop called. Current state: " << static_cast<int>(m_currentState);
    AccumulatorState previousState = m_currentState;

    m_accumulationTimer->stop();
    m_continuousIntervalTimer->stop();
    m_curResult = AccumulationResult();

    if (previousState == AccumulatorState::Measuring) {
        internalStopAccumulation(false);
    } else if (previousState == AccumulatorState::Waiting || previousState == AccumulatorState::Completed) {
        transitionToState(AccumulatorState::Idle);
    } else {
        transitionToState(AccumulatorState::Idle);
    }
}

void SpectrumAccumulator::internalStartAccumulation() {
    if(m_currentState != AccumulatorState::Waiting) {
        nucare::logW() << "SpectrumAccumulator: internalStartAccumulation called from unexpected state: " << static_cast<int>(m_currentState);
        transitionToState(AccumulatorState::Idle);
        return;
    }

    nucare::logI() << "SpectrumAccumulator: Internal accumulation starting. Mode: " << static_cast<int>(m_mode) << ", ActiveType: " << static_cast<int>(m_activeAccumulationType);

    m_curResult.startTime = QDateTime::currentDateTime();
    m_curResult.finishTime = m_curResult.startTime.addSecs(m_timeoutValueSeconds); // Initialize finish time
//    m_currentResultSnapshot.finishTime = QDateTime::fromSecsSinceEpoch(m_currentResultSnapshot.startTime.toSecsSinceEpoch() + m_timeoutValueSeconds);

    m_curResult.executionRealtimeSeconds = 0;
    m_curResult.cps = 0.0;
    m_curResult.activeType = m_activeAccumulationType; // Ensure activeType is set in snapshot

    if (m_activeAccumulationType == ActiveSpectrumType::TypeSpectrum) {
        m_curResult.spectrum = std::make_shared<Spectrum>();
        m_curResult.hwSpectrum = nullptr; // Clear other type
    } else if (m_activeAccumulationType == ActiveSpectrumType::TypeHwSpectrum) {
        m_curResult.hwSpectrum = std::make_shared<HwSpectrum>();
        m_curResult.spectrum = nullptr; // Clear other type
    } else {
        nucare::logE() << "SpectrumAccumulator: Cannot start, active spectrum type is None. Mode: " << static_cast<int>(m_mode);
        transitionToState(AccumulatorState::Idle);
        return;
    }
    transitionToState(AccumulatorState::Measuring);

    bool needsTimeout = false;
    if (m_mode == AccumulationMode::ByTime || m_mode == AccumulationMode::ContinuousByTime) {
        needsTimeout = true;
    } else if ((m_mode == AccumulationMode::ByCount || m_mode == AccumulationMode::ContinuousByCount) && m_timeoutValueSeconds > 0) {
        needsTimeout = true;
    }

    if (needsTimeout) {
        if (m_timeoutValueSeconds > 0) {
            m_accumulationTimer->start(m_timeoutValueSeconds * 1000);
            nucare::logI() << "SpectrumAccumulator: Accumulation timer started for " << m_timeoutValueSeconds << " seconds.";
        } else {
            nucare::logW() << "SpectrumAccumulator: Time-dependent mode selected but timeout is zero or negative. Accumulation may not stop as expected.";
        }
    }
}

void SpectrumAccumulator::internalStopAccumulation(bool conditionMet) {
    if (m_currentState != AccumulatorState::Measuring) {
        nucare::logW() << "SpectrumAccumulator: InternalStop called but not in Measuring state. Current state: " << static_cast<int>(m_currentState);
        m_accumulationTimer->stop();
        return;
    }

    m_accumulationTimer->stop();

    if (conditionMet) {
        // Condition met, transaction completed

        m_curResult.executionRealtimeSeconds = static_cast<double>(m_curResult.startTime.msecsTo(m_curResult.finishTime)) / 1000.0;

        double finalTotalCount = 0;
        if (m_activeAccumulationType == ActiveSpectrumType::TypeSpectrum && m_curResult.spectrum) {
            finalTotalCount = static_cast<double>(m_curResult.spectrum->getTotalCount());
        } else if (m_activeAccumulationType == ActiveSpectrumType::TypeHwSpectrum && m_curResult.hwSpectrum) {
            finalTotalCount = static_cast<double>(m_curResult.hwSpectrum->getTotalCount());
        } else {
            nucare::logW() << "SpectrumAccumulator: No spectrum data in snapshot or type mismatch on stop. ActiveType: " << static_cast<int>(m_activeAccumulationType);
        }
        m_curResult.cps = (m_curResult.executionRealtimeSeconds > 0.00001) ? (finalTotalCount / m_curResult.executionRealtimeSeconds) : 0.0;

        if (m_activeAccumulationType == ActiveSpectrumType::TypeSpectrum && m_curResult.spectrum) {
            m_curResult.spectrum->setCountRate(m_curResult.cps);
        }
        // Note: HwSpectrum does not have setCountRate. If it needs a similar field, it would require changes to HwSpectrum.

        // Populate detector, background, and calibration IDs
        auto ncManager = ComponentManager::instance().ncManager();
        if (ncManager) {
            auto currentDetector = ncManager->getCurrentDetector();
            if (currentDetector) {
                auto prop = currentDetector->properties();
                m_curResult.detectorId = prop->getId();
                if (m_curResult.spectrum) {
                    m_curResult.spectrum->setDetectorID(m_curResult.detectorId);
                } else if (m_curResult.hwSpectrum) {
                    m_curResult.hwSpectrum->setDetectorID(m_curResult.detectorId);
                }

                nucare::logD() << "Set detectorId in AccumulationResult: " << m_curResult.detectorId;

                if (auto bgr = prop->getBackground()) {
                    m_curResult.backgroundId = bgr->id;
                    nucare::logD() << "Set backgroundId in AccumulationResult: " << m_curResult.backgroundId;
                } else {
                    m_curResult.backgroundId = -1;
                }

                if (auto calib = prop->getCalibration()) {
                    m_curResult.calibrationId = calib->getId();
                } else {
                    m_curResult.calibrationId = -1;
                }
            } else {
                m_curResult.detectorId = -1;
                m_curResult.backgroundId = -1;
                m_curResult.calibrationId = -1;
            }
        } else {
            m_curResult.detectorId = -1;
            m_curResult.backgroundId = -1;
            m_curResult.calibrationId = -1;
            nucare::logW() << "NcManager instance not found! Setting all IDs in AccumulationResult to -1.";
        }

        transitionToState(AccumulatorState::Completed);
    }

    nucare::logI() << "SpectrumAccumulator: Internal accumulation stopped. Condition met: " << conditionMet << ". Final CPS: " << m_curResult.cps;
    // No accumulationComplete signal. Clients observe state and call getCurrentAccumulationResult().

    if ((m_mode == AccumulationMode::ContinuousByCount || m_mode == AccumulationMode::ContinuousByTime) && conditionMet) {
        transitionToState(AccumulatorState::Waiting);
        if (m_continuousIntervalSeconds > 0) {
            m_continuousIntervalTimer->start(m_continuousIntervalSeconds * 1000);
            nucare::logI() << "SpectrumAccumulator: Continuous mode: interval timer started for " << m_continuousIntervalSeconds << " seconds.";
        } else {
            nucare::logI() << "SpectrumAccumulator: Continuous mode with 0s interval, restarting immediately.";
            internalStartAccumulation();
        }
    } else {
        transitionToState(AccumulatorState::Idle);
    }
}

void SpectrumAccumulator::onNcManagerSpectrumReceived(std::shared_ptr<Spectrum> spcFromSignal) {
    if (m_currentState != AccumulatorState::Measuring) {
        return;
    }
    bool accumulatedSomething = false;
    nucare::DetectorComponent* detComp = ComponentManager::instance().detectorComponent();
    if (!detComp) {
        return;
    }

    auto prop = detComp->properties();

    if (m_activeAccumulationType == ActiveSpectrumType::TypeSpectrum) {
        if (spcFromSignal && m_curResult.spectrum) {
            m_curResult.spectrum->accumulate(*spcFromSignal);
            accumulatedSomething = true;
        } else if (!spcFromSignal) {
            nucare::logW() << "SpectrumAccumulator: Received null Spectrum in ByTime mode from NcManager.";
        } else {
            nucare::logE() << "SpectrumAccumulator: Snapshot spectrum (Spectrum type) is null in ByTime mode.";
        }
    } else if (m_activeAccumulationType == ActiveSpectrumType::TypeHwSpectrum) {
        std::shared_ptr<HwSpectrum> originHwSpc = detComp->properties()->getOriginSpc();
        if (originHwSpc && m_curResult.hwSpectrum) {
            m_curResult.hwSpectrum->accumulate(*originHwSpc);
            accumulatedSomething = true;
        } else if (!originHwSpc) {
            nucare::logW() << "SpectrumAccumulator: Failed to get HwSpectrum from DetectorProperty::getOriginSpc() in ByCount mode (nullptr received).";
        } else {
            nucare::logE() << "SpectrumAccumulator: Snapshot spectrum (HwSpectrum type) is null in ByCount mode.";
        }
    } else {
        nucare::logW() << "SpectrumAccumulator: Data received but active spectrum type is None. Mode: " << static_cast<int>(m_mode);
        return;
    }

    if(accumulatedSomething) {
        m_curResult.executionRealtimeSeconds = static_cast<double>(m_curResult.startTime.msecsTo(QDateTime::currentDateTime())) / 1000.0;
        m_curResult.cps = prop->getCps();
        m_curResult.count++;
        m_curResult.maxCPS = std::max(m_curResult.maxCPS, m_curResult.cps);
        m_curResult.minCPS = std::min(m_curResult.minCPS, m_curResult.cps);
        // Tính avgCPS bằng running average theo số lần tích lũy
        m_curResult.avgCPS = m_curResult.avgCPS + (m_curResult.cps - m_curResult.avgCPS) / static_cast<double>(m_curResult.count);
        emit accumulationUpdated();
    }

    if (m_activeAccumulationType == ActiveSpectrumType::TypeHwSpectrum && m_curResult.hwSpectrum) {
        if (m_targetCountValue > 0 && m_curResult.hwSpectrum->getTotalCount() >= m_targetCountValue) {
            nucare::logI() << "SpectrumAccumulator: Target count " << m_targetCountValue << " reached.";
            internalStopAccumulation(true);
        }
    }
}

void SpectrumAccumulator::onAccumulationTimeout() {
    if (m_currentState != AccumulatorState::Measuring) {
        nucare::logW() << "SpectrumAccumulator: AccumulationTimeout received but not in Measuring state. State: " << static_cast<int>(m_currentState);
        return;
    }
    nucare::logI() << "SpectrumAccumulator: Accumulation cycle timeout reached.";

    if (m_mode == AccumulationMode::ByTime || m_mode == AccumulationMode::ContinuousByTime) {
        internalStopAccumulation(true);
    } else if (m_mode == AccumulationMode::ByCount || m_mode == AccumulationMode::ContinuousByCount) {
        internalStopAccumulation(false);
    } else {
         nucare::logW() << "SpectrumAccumulator: Timeout in unhandled mode " << static_cast<int>(m_mode);
         internalStopAccumulation(false);
    }
}

void SpectrumAccumulator::onContinuousIntervalTimeout() {
    if (m_currentState == AccumulatorState::Waiting &&
        (m_mode == AccumulationMode::ContinuousByCount || m_mode == AccumulationMode::ContinuousByTime)) {
        nucare::logI() << "SpectrumAccumulator: Continuous interval timeout. Restarting accumulation.";
        internalStartAccumulation();
    } else {
        nucare::logW() << "SpectrumAccumulator: Continuous interval timeout received in unexpected state " << static_cast<int>(m_currentState) << " or mode " << static_cast<int>(m_mode);
    }
}

void SpectrumAccumulator::adjustTargetTime(int secondsDelta) {
    if (m_activeAccumulationType != ActiveSpectrumType::TypeSpectrum) {
        nucare::logW() << "SpectrumAccumulator: adjustTargetTime called but not in a time-based accumulation type (TypeSpectrum). ActiveType: " << static_cast<int>(m_activeAccumulationType);
        return;
    }
    setTargetTime(m_timeoutValueSeconds + secondsDelta);
}

void SpectrumAccumulator::adjustTargetCount(int countDelta) {
    if (m_activeAccumulationType != ActiveSpectrumType::TypeHwSpectrum) {
        nucare::logW() << "SpectrumAccumulator: adjustTargetCount called but not in a count-based accumulation type (TypeHwSpectrum). ActiveType: " << static_cast<int>(m_activeAccumulationType);
        return;
    }

    int newTargetCount = m_targetCountValue + countDelta;
    if (newTargetCount < 1) {
        newTargetCount = 1;
        nucare::logW() << "SpectrumAccumulator: Adjusted count resulted in less than 1, setting to 1.";
    }
    m_targetCountValue = newTargetCount;
    nucare::logI() << "SpectrumAccumulator: Target count adjusted by " << countDelta << ". New target: " << m_targetCountValue;

    if (m_currentState == AccumulatorState::Measuring) {
        if (m_curResult.hwSpectrum && m_curResult.hwSpectrum->getTotalCount() >= m_targetCountValue) {
            nucare::logI() << "SpectrumAccumulator: Count adjustment resulted in target count already being met.";
            internalStopAccumulation(true);
        } else if (!m_curResult.hwSpectrum) {
            nucare::logE() << "SpectrumAccumulator: adjustTargetCount: Snapshot HwSpectrum is null in Measuring state.";
        }
    }
}

void SpectrumAccumulator::setTargetCount(int newTargetCount) {
    if (m_activeAccumulationType != ActiveSpectrumType::TypeHwSpectrum) {
        nucare::logW() << "SpectrumAccumulator: adjustTargetCount called but not in a count-based accumulation type (TypeHwSpectrum). ActiveType: " << static_cast<int>(m_activeAccumulationType);
        return;
    }

    if (newTargetCount < 1) {
        newTargetCount = 1;
        nucare::logW() << "SpectrumAccumulator: Adjusted count resulted in less than 1, setting to 1.";
    }
    m_targetCountValue = newTargetCount;
    nucare::logI() << "SpectrumAccumulator: Target count set to " << m_targetCountValue;

    if (m_currentState == AccumulatorState::Measuring) {
        if (m_curResult.hwSpectrum && m_curResult.hwSpectrum->getTotalCount() >= m_targetCountValue) {
            nucare::logI() << "SpectrumAccumulator: Count adjustment resulted in target count already being met.";
            internalStopAccumulation(true);
        } else if (!m_curResult.hwSpectrum) {
            nucare::logE() << "SpectrumAccumulator: adjustTargetCount: Snapshot HwSpectrum is null in Measuring state.";
        }
    }
}

void SpectrumAccumulator::setTargetTime(int time)
{
    if (m_activeAccumulationType != ActiveSpectrumType::TypeSpectrum) {
        nucare::logW() << "SpectrumAccumulator: setTargetTime called but not in a time-based accumulation type (TypeSpectrum). ActiveType: " << static_cast<int>(m_activeAccumulationType);
        return;
    }

    if (time < 1) {
        time = 1;
        nucare::logW() << "SpectrumAccumulator: Set target time resulted in less than 1s, setting to 1s.";
    }
    m_timeoutValueSeconds = time;
    nucare::logI() << "SpectrumAccumulator: Target time set to " << m_timeoutValueSeconds << "s.";

    if (m_currentState == AccumulatorState::Measuring && m_accumulationTimer->isActive()) {
        m_curResult.finishTime = m_curResult.startTime.addSecs(m_timeoutValueSeconds);
        qint64 elapsedMsecs = m_curResult.startTime.msecsTo(QDateTime::currentDateTime());
        qint64 newTotalMsecs = static_cast<qint64>(m_timeoutValueSeconds) * 1000;
        qint64 remainingMsecs = newTotalMsecs - elapsedMsecs;

        m_accumulationTimer->stop();
        if (remainingMsecs <= 0) {
            nucare::logI() << "SpectrumAccumulator: Time set resulted in timeout already being met or passed.";
            onAccumulationTimeout();
        } else {
            m_accumulationTimer->start(remainingMsecs);
            nucare::logI() << "SpectrumAccumulator: Accumulation timer restarted with remaining " << remainingMsecs << " ms.";
        }
    }
}

void SpectrumAccumulator::setIntervalTime(int time)
{
    if (time < 0) {
        time = 0;
        nucare::logW() << "SpectrumAccumulator: Set interval time resulted in less than 0s, setting to 0s.";
    }
    m_continuousIntervalSeconds = time;
    nucare::logI() << "SpectrumAccumulator: Interval time set to " << m_continuousIntervalSeconds << "s.";

    if (m_currentState == AccumulatorState::Waiting &&
        (m_mode == AccumulationMode::ContinuousByCount || m_mode == AccumulationMode::ContinuousByTime)) {
        m_continuousIntervalTimer->stop();
        if (m_continuousIntervalSeconds > 0) {
            m_continuousIntervalTimer->start(m_continuousIntervalSeconds * 1000);
            nucare::logI() << "SpectrumAccumulator: Continuous interval timer restarted for " << m_continuousIntervalSeconds << " seconds.";
        } else {
            nucare::logI() << "SpectrumAccumulator: Continuous mode with 0s interval, restarting immediately.";
            internalStartAccumulation();
        }
    }
}
