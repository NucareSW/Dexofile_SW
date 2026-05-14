#include "detectorcomponent.h"
#include "model/DetectorProp.h"
#include "model/DetectorCalibConfig.h"
#include "component/databasemanager.h"
#include "component/componentmanager.h"
#include "util/util.h" // For logging
#include "util/nc_exception.h"
#include "util/NcLibrary.h"

using namespace std;

namespace nucare {

// Define the Payload struct for detector info
union GcPackage {
    char Bytes[28]; // Total size of the payload
    struct Payload {
        char header[2];     // GD
        uint16_t gain;
        uint16_t k40Ch;
        uint16_t detectorCode;
        uint16_t ch32Kev;
        uint16_t ch662Kev;
        char padding1[4];
        char temperatureFlag;
        uint16_t temperature;
        char serial[6];
        char padding2;
        char tail[2];
    } __attribute__((__packed__));
};

static_assert (sizeof(GcPackage::Payload) == sizeof(GcPackage::Bytes), "Invalid size for GcPackage union");


// Define the Package union for continuous data
union Package {
    char Bytes[4166]; // 4(header)+4096(spectrum)+2(offset1)+2(neutron)+6(realtime)+1(neutronGmFlag)+1(padding)+2(pileup)+2(temperatureChip)+8(padding3)+2(timestamp)+2(padding4)+2(detectorCode)+2(hvDac)+2(gc)+2(hvAdc)+2(ch32kev)+2(ch662kev)+2(ch1460kev)+2(tempOutside)+2(gm)+2(tail) = 4166
    struct Payload {
        char header[4]; // UUD0
        uint16_t spectrum[2048];
        char offset1[2]; // 33
        uint16_t neutron;
        uint64_t realtime : 48;
        uint8_t neutronGmFlag;
        uint8_t padding;
        uint16_t pileup;
        uint16_t temperatureChip;
        uint16_t padding3[4];
        uint16_t timestamp;
        uint16_t padding4;
        uint16_t detectorCode;
        uint16_t hvDac;
        uint16_t gc;
        uint16_t hvAdc;
        uint16_t ch32kev;
        uint16_t ch662kev;
        uint16_t ch1460kev;
        uint16_t tempOutside;
        uint16_t gm;
        char tail[20]; // tail is 20 bytes, last 2 bytes must be FF FF
    } __attribute__((__packed__));
};

static_assert (sizeof(Package::Payload) == sizeof (Package::Bytes), "Invalid size for package");

struct UpdateCalibReq {
    char Bytes[28];
    struct Payload {
        const char header[2] = {'C', 'L'};
        uint16_t gain;
        uint16_t temperature;
        uint16_t ch32Kev;
        uint16_t ch662Kev;
        uint16_t chK40;
        const uint16_t hvDac = 0;
        const char padding[12] = {};
        const char tail[2] = {'6', '6'};
    } __attribute__((__packed__));
};

static_assert (sizeof (UpdateCalibReq) == 28, "Invalid size of message UpdateCalibReq.");

constexpr char PACKAGE_HEADER[4] = {'U', 'U', 'D', '0'};
constexpr char PACKAGE_TAIL[2] = {'6', '6'};

std::shared_ptr<DetectorPackage> detector_raw_package_convert(const Package::Payload* pkg) {
    auto ret = make_shared<DetectorPackage>();
    auto payload = reinterpret_cast<const Package::Payload*>(pkg);

    //read realtime
    ret->realtime = be64toh(payload->realtime << 16) * 0.00002;
    double msTime = 1;
    if(ret->realtime > 1) msTime = ret->realtime;

    // Spectrum
    ret->spc = make_shared<HwSpectrum>();
    for (int i = 0; i < ret->spc->getSize(); i++) {
        auto count = be16toh(payload->spectrum[i]) / msTime;

        if (count > 15000) {
            count = 0;
        }

        (*ret->spc)[i] = count;
    }
    ret->spc->update();

    ret->neutron = be16toh(payload->neutron);
    ret->hasNeutron = payload->neutronGmFlag >> 7;
    ret->hasGM = payload->neutronGmFlag & 0x40;

    ret->pileup = be16toh(payload->pileup);
    ret->spc->setFillCps(ret->pileup);
    ret->detectorInfo = be16toh(payload->detectorCode);
    ret->hvDac = be16toh(payload->hvDac);
    ret->gc = be16toh(payload->gc);
    // GM
    ret->gm = be16toh(payload->gm);

    ret->temperatureRaw = be16toh(payload->tempOutside);
//    ret->temperature = (temp * 2.0 / 4096.0 - 0.5) * 100;
    ret->temperature = (ret->temperatureRaw * 330) / 4096  - 60;

    return ret;
}

DetectorComponent::DetectorComponent(QObject *parent)
    : Component("DETECTOR"), m_properties(make_shared<DetectorProperty>()), m_serialPort(new QSerialPort(this)),
      m_retryCount(0), m_commandState(Idle), m_packageBytes(0)
{
    memset(m_packageBuffer, 0, sizeof(m_packageBuffer));
    // Connect serial port signals
    connect(m_serialPort, &QSerialPort::readyRead, this, &DetectorComponent::readData);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &DetectorComponent::handleError);

    // Connect timer signal
    connect(&m_responseTimer, &QTimer::timeout, this, &DetectorComponent::handleTimeout);
    m_responseTimer.setSingleShot(true); // Timer for response timeouts
}

DetectorComponent::~DetectorComponent()
{
    closeSerialPort();
}

void DetectorComponent::open(const QString& portName)
{
    logI() << "Initializing DetectorComponent.";
    int baudRate = 230400; // TODO: Load from settings

    if (openSerialPort(portName, baudRate)) {
        logI() << "Serial port" << portName << "opened successfully.";
        clearSerialBuffer(); // Clear buffer on successful open
    } else {
        logE() << "Failed to open serial port" << portName << ":" << m_serialPort->errorString();
        emit errorOccurred(m_serialPort->errorString());
    }
}

void DetectorComponent::clearSerialBuffer()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->clear();
        logI() << "Serial buffer cleared.";
    }
}

void DetectorComponent::start()
{
    logI() << "Starting DetectorComponent command flow.";
    startCommandFlow();
}

void DetectorComponent::stop()
{
    logI() << "Stopping DetectorComponent.";
    closeSerialPort();
    m_responseTimer.stop();
    m_commandState = Idle;
    m_readBuffer.clear();
    m_retryCount = 0;
}

bool DetectorComponent::openSerialPort(const QString &portName, int baudRate)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    return m_serialPort->open(QIODevice::ReadWrite);
}

void DetectorComponent::closeSerialPort()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        logI() << "Serial port closed.";
    }
}

void DetectorComponent::sendCommand(const QByteArray &command)
{
    if (m_serialPort->isOpen()) {
        nativeSend(command);
        m_currentCommand = command;
        m_readBuffer.clear(); // Clear buffer for new response
        m_responseTimer.start(1000); // Start timeout timer (1 second)
    } else {
        logE() << "Cannot send command, serial port not open.";
        emit errorOccurred("Cannot send command, serial port not open.");
    }
}

void DetectorComponent::nativeSend(const QByteArray &command)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->write(command);
        logD() << "Sent command:" << command;
    }
}

void DetectorComponent::initialize()
{
    auto db = ComponentManager::instance().databaseManager();
    if (!db) {
        NC_THROW_ARG_ERROR("Not found any data");
    }

    m_properties->setBackgroundSpc(db->getLatestBackground(m_properties->getId()));

    // Initilialize Calibration
    auto oldCalib = m_properties->getCalibration();
    auto ret = db->getLatestCalibration(m_properties->getId());
    auto calibConfig = db->getDefaultDetectorConfig(m_properties->getId());
    if (ret == nullptr) {

        ret = make_shared<Calibration>();

        ret->setDetectorId(m_properties->getId());

        Coeffcients& foundPeaks = calibConfig->calib;
        if (oldCalib != nullptr) {
            auto hwCoeff = oldCalib->chCoefficients();
            if (hwCoeff[0] > 0 && hwCoeff[1] > 0 && hwCoeff[2] > 0) {
                foundPeaks = hwCoeff;
                ret->setGC(m_properties->getGC());
            }
        }

        ret->setDate(nucare::Timestamp());

        const int size = 3;
        std::array<double, size> enPeaks = {CS137_PEAK1, CS137_PEAK2, K40_PEAK};

        auto coeff = NcLibrary::computeCalib(foundPeaks, enPeaks);
        Coeffcients convCoef;
        auto ratio = NcLibrary::calibConvert(coeff.data(), convCoef.data(), 2048, nucare::CHSIZE);
        ret->setRatio(ratio);
        ret->setCoefficients(convCoef);

        ret->setChCoefficients({
                                   foundPeaks[0] / ratio,
                                   foundPeaks[1] / ratio,
                                   foundPeaks[2] / ratio
                               });

        db->insertCalibration(ret.get());
    }

    ret->setStdPeaks(calibConfig->calib);
    if (ret->getGC() == 0) {
        ret->setGC(m_properties->getGC());
    }

    m_properties->setCalibration(ret);
    m_properties->m_initialized = true;

    emit detectorInitialized();
}

void DetectorComponent::readData()
{
    m_readBuffer.append(m_serialPort->readAll());
    logD() << "Received data. Current buffer size:" << m_readBuffer.size();
    processReceivedData();
}

void DetectorComponent::handleTimeout()
{
    logW() << "Response timeout for command:" << m_currentCommand;
    m_retryCount++;
    if (m_commandState == WaitingForStopResponse) {
        // For Stop, timeout means success (no data expected)
        logI() << "Stop command acknowledged by timeout (no data expected).";
        m_retryCount = 0;
        sendGetInfoCommand();
        return;
    }
    if (m_retryCount < 3) {
        logI() << "Retrying command:" << m_currentCommand << "(Attempt" << m_retryCount + 1 << "of 3)";
        sendCommand(m_currentCommand); // Retry the command
    } else if (m_commandState == WaitingForInfoResponse) {
        logE() << "Failed after three times, load last detector info instead";
        auto db = ComponentManager::instance().databaseManager();
        if (!db) {
            emit errorOccurred("Database is unavailable");
            return;
        }

        auto info = db->getLastDetector();
        if (!info) {
            emit errorOccurred("Not found any detector");
            return;
        }

        m_properties->info = *info;
        initialize();

    } else {
        logE() << "Command failed after 3 retries:" << m_currentCommand;
        emit errorOccurred("Command failed after 3 retries.");
        m_commandState = Idle; // Reset state
        m_retryCount = 0;
    }
}

void DetectorComponent::handleError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        logE() << "Serial port error:" << m_serialPort->errorString();
        emit errorOccurred(m_serialPort->errorString());
    }
}

void DetectorComponent::processReceivedData()
{
    // Improved buffer processing: handle partial, multiple, and misaligned messages
    while (true) {
        switch (m_commandState) {
            case WaitingForInfoResponse: {
                int headerIndex = m_readBuffer.indexOf("GD");
                if (headerIndex < 0) break;
                if (m_readBuffer.size() < headerIndex + sizeof(GcPackage::Payload)) break;
                QByteArray payloadData = m_readBuffer.mid(headerIndex, sizeof(GcPackage::Payload));
                processDetectorInfo(payloadData);
                m_readBuffer.remove(0, headerIndex + sizeof(GcPackage::Payload));
                m_responseTimer.stop();
                m_retryCount = 0;
                sendStartCommand();
                continue;
            }
            case ReceivingPackage: {
                while (!m_readBuffer.isEmpty()) {
                    if (m_packageBytes == 0) {
                        // Not currently accumulating, search for header
                        int headerIndex = m_readBuffer.indexOf(QByteArray(PACKAGE_HEADER, 4));
                        if (headerIndex < 0) {
                            if (m_readBuffer.size() > 3)
                                m_readBuffer = m_readBuffer.right(3);
                            break;
                        }
                        if (headerIndex > 0) {
                            // There is data before the header: treat as tail of previous package if enough bytes
                            int tailBytes = headerIndex;
                            if (m_packageBytes > 0 && (m_packageBytes + tailBytes) >= sizeof(Package)) {
                                int bytesNeeded = sizeof(Package) - m_packageBytes;
                                int bytesToCopy = std::min(bytesNeeded, tailBytes);
                                memcpy(m_packageBuffer + m_packageBytes, m_readBuffer.constData(), bytesToCopy);
                                m_packageBytes += bytesToCopy;
                                if (m_packageBytes == sizeof(Package)) {
                                    processPackageData(QByteArray(m_packageBuffer, sizeof(Package)));
                                    m_packageBytes = 0;
                                }
                            }
                            // Remove up to the header for new package
                            m_readBuffer.remove(0, headerIndex);
                        }
                        int bytesToCopy = std::min((int)m_readBuffer.size(), (int)sizeof(Package));
                        memcpy(m_packageBuffer, m_readBuffer.constData(), bytesToCopy);
                        m_packageBytes = bytesToCopy;
                        m_readBuffer.remove(0, bytesToCopy);
                        if (m_packageBytes == sizeof(Package)) {
                            processPackageData(QByteArray(m_packageBuffer, sizeof(Package)));
                            m_packageBytes = 0;
                            continue;
                        }
                        break;
                    } else {
                        int bytesNeeded = sizeof(Package) - m_packageBytes;
                        int bytesToCopy = std::min(bytesNeeded, m_readBuffer.size());
                        memcpy(m_packageBuffer + m_packageBytes, m_readBuffer.constData(), bytesToCopy);
                        m_packageBytes += bytesToCopy;
                        m_readBuffer.remove(0, bytesToCopy);
                        if (m_packageBytes == sizeof(Package)) {
                            processPackageData(QByteArray(m_packageBuffer, sizeof(Package)));
                            m_packageBytes = 0;
                            continue;
                        }
                        break;
                    }
                }
                break;
            }
            case WaitingForStopResponse: {
                // After sending Stop, we expect NO data. Timeout means success.
                // If any data is received, log a warning and clear it, but do not treat as success.
                if (!m_readBuffer.isEmpty()) {
                    logW() << "Unexpected data received after sending Stop command. Clearing buffer.";
                    m_readBuffer.clear();
                }
                // Do not trigger sendGetInfoCommand here; let the timeout handler advance the state.
                break;
            }
            case WaitingForStartResponse: {
                if (!m_readBuffer.isEmpty()) {
                    logI() << "Response received after sending Start command. Entering ReceivingPackage state.";
                    m_responseTimer.stop();
                    m_retryCount = 0;
                    m_commandState = ReceivingPackage;
//                    m_readBuffer.clear();
                    processReceivedData();  // If received data in start response, forward this buffer for next step
                }
                break;
            }
            case Idle: {
                if (!m_readBuffer.isEmpty()) {
                    logW() << "Received unexpected data in Idle state." << m_readBuffer.toHex();
                    m_readBuffer.clear();
                }
                break;
            }
            case SendingStop:
            case SendingGetInfo:
            case SendingStart:
                // Should not process data while just sending, wait for response state.
                break;
        }
        // If we didn't continue, break the loop
        break;
    }
}

void DetectorComponent::processDetectorInfo(const QByteArray &data)
{
    // Process the received detector info (28 bytes)
    if (data.size() == sizeof(GcPackage::Payload)) {
        const GcPackage::Payload* rawPayload = reinterpret_cast<const GcPackage::Payload*>(data.constData());
        // Validate header 'GD' - already done in processReceivedData, but double-check
        if (rawPayload->header[0] == 'G' && rawPayload->header[1] == 'D') {
            logI() << "Successfully processed detector info.";

            auto gcResponse            = std::make_shared<GcResponse>();
            gcResponse->gc             = be16toh(rawPayload->gain);
            gcResponse->k40Ch          = be16toh(rawPayload->k40Ch);
            gcResponse->detType        = be16toh(rawPayload->detectorCode);
            gcResponse->cs137Ch1       = be16toh(rawPayload->ch32Kev);
            gcResponse->cs137Ch2       = be16toh(rawPayload->ch662Kev);
            gcResponse->hasTemperature = rawPayload->temperatureFlag == 'T';
            if (gcResponse->hasTemperature) {
                gcResponse->temperature = rawPayload->temperature;
            }

            gcResponse->serial = QString::fromLocal8Bit(rawPayload->serial, sizeof(rawPayload->serial));

            m_properties->setSerial(gcResponse->serial);
            m_properties->setDetectorCode((DetectorCode_E) gcResponse->detType);
            auto det = ComponentManager::instance().databaseManager()->getDetectorByCriteria(
                        m_properties->getSerial(),
                        m_properties->info.model,
                        m_properties->info.probeType,
                        QString::number(m_properties->info.detectorCode->code),
                        QString::number(m_properties->info.detectorCode->cType)
                        );
            if (det) {
                m_properties->info = *det;
            }

            initialize();

            emit detectorInfoReceived(this, gcResponse);
        } else {
            logE() << "Detector info validation failed: Incorrect header.";
            emit errorOccurred("Detector info validation failed: Incorrect header.");
        }
    } else {
        logE() << "Detector info validation failed: Incorrect size (" << data.size() << " bytes, expected" << sizeof(GcPackage::Payload) << ").";
        emit errorOccurred("Detector info validation failed: Incorrect size.");
    }
}


void DetectorComponent::processPackageData(const QByteArray &data)
{
    // Process the received package data (4166 bytes)
    if (data.size() == sizeof(Package::Bytes)) {
        auto rawPackage = reinterpret_cast<const Package::Payload*>(data.constData());
        // Use constexpr header/tail for fast check
        bool headerOk = (memcmp(rawPackage->header, PACKAGE_HEADER, 4) == 0);
        bool tailOk = (memcmp(rawPackage->tail + 18, PACKAGE_TAIL, 2) == 0);
        if (headerOk && tailOk) {
            std::shared_ptr<DetectorPackage> detectorPackage = detector_raw_package_convert(rawPackage);
            emit packageReceived(this, detectorPackage);
        } else {
            logW() << "Package data validation failed: Incorrect header or tail.";
        }
    } else {
        logW() << "Package data validation failed: Incorrect size (" << data.size() << " bytes, expected" << sizeof(Package::Bytes) << ").";
    }
}

void DetectorComponent::startCommandFlow()
{
    if (m_commandState == Idle) {
        sendStopCommand();
    } else {
        logW() << "Command flow already in progress. Current state:" << m_commandState;
    }
}

void DetectorComponent::sendStopCommand()
{
    logI() << "Sending Stop command (A4).";
    m_commandState = SendingStop;
    sendCommand("A4");
    m_commandState = WaitingForStopResponse;
}

void DetectorComponent::sendGetInfoCommand()
{
    logI() << "Sending Get Info command (GS).";
    m_commandState = SendingGetInfo;
    sendCommand("GS");
    m_commandState = WaitingForInfoResponse;
}

void DetectorComponent::sendStartCommand()
{
    logI() << "Sending Start command (A2).";
    m_commandState = SendingStart;
    sendCommand("A2");
    m_commandState = WaitingForStartResponse;
}

void DetectorComponent::sendUpdateCalib(const int ch32, const int ch662, const int chK40)
{
    logD() << "Sending update calib: " << ch32 << ',' << ch662 << ',' << chK40;
    UpdateCalibReq::Payload payload;

//    auto payload = reinterpret_cast<UpdateCalibReq::Payload*>(buf.data());
    payload.gain = htobe16(m_properties->getGC());
    payload.temperature = htobe16(m_properties->getRawTemperature());
    payload.ch32Kev = htobe16(ch32);
    payload.ch662Kev = htobe16(ch662);
    payload.chK40 = htobe16(chK40);

    QByteArray buf(reinterpret_cast<char*>(&payload), sizeof(payload));
    nativeSend(buf);
}

void DetectorComponent::sendPowerOff() {
    nativeSend("PD");
}

} // namespace nucare
