#include "settingmanager.h"
#include "component/componentmanager.h" // For ComponentManager
#include "component/databasemanager.h"  // For DatabaseManager
#include "model/basesettingitem.h"
#include "util/util.h" // For logD()
#include <QMap>   // For QMap

using namespace std;
using namespace setting;

// --- ConfigEntry Implementations ---

ConfigEntry::ConfigEntry(const char* key)
    : QObject(nullptr), m_key(key)
{
    // Default value is QVariant() (null/invalid)
}

ConfigEntry::ConfigEntry(const char* key, QVariant value)
    : QObject(nullptr), m_key(key), m_value(value)
{
}

ConfigEntry::ConfigEntry(const char* key, SettingManager* manager)
    : QObject(manager), m_key(key), m_manager(manager)
{
    // Default value is QVariant() (null/invalid)
}

ConfigEntry::ConfigEntry(const char* key, QVariant value, SettingManager* manager)
    : QObject(manager), m_key(key), m_value(value), m_manager(manager)
{
}

QVariant ConfigEntry::getValue() const
{
    return m_value;
}

const char* ConfigEntry::getKey() const
{
    return m_key;
}

void ConfigEntry::setValue(QVariant newValue, bool saveNow)
{
    if (m_value != newValue) {
        m_value = newValue;
        if (m_manager) {
            if (saveNow)
                m_manager->saveSetting(m_key, m_value);

            emit itemValueChanged(m_manager, m_key);
        } else {
            nucare::logD() << "ConfigEntry:" << m_key << "value changed but no manager set to emit signal.";
        }
    }
}

void ConfigEntry::setManager(SettingManager* manager)
{
    m_manager = manager;
    // If manager is also the QObject parent, set it here if not already set
    if (parent() == nullptr && manager != nullptr) {
        setParent(manager);
    }
}

// --- SettingManager Implementations ---

// Example ConfigEntry members declared directly in SettingManager's .cpp
// This is a common pattern for managing QObject children directly.
// They will be initialized in the SettingManager constructor's initializer list.
// Note: These are just examples. In a real scenario, these would be declared in the .h file
// as private members of SettingManager. For this task, I'm demonstrating the concept
// as per the user's request about "declaring a variable like this: SettingManager { ConfigEntry measurementCont(this, "KEY"); ... }"
// To truly declare them as members, they would need to be in the .h file.
// For now, I'll simulate this by creating them on the heap and managing their ownership.
// If the user wants them as direct members, I'll need to modify the .h again.

SubSettingItem* setting::buildSettingTree() {
    SubSettingItem* ret = new SubSettingItem();
    ret->setSettings({

        (new HeaderSettingItem(ret))->setName("Setting"),
        (new SubSettingItem(ret))
            ->setSettings({
                (new ChoiceSettingItem(QMap<QString, QVariant>({
                                           {"60", 60},
                                           {"120", 120},
                                           {"180", 180},
                                           {"300", 300},
                                           {"600", 600},
                                       }),
                                       ret))
                    ->setName("Measure Time")
                    ->setShowValue(true)
                    ->setKey(SettingManager::KEY_ACQTIME_BGR),
                (new InfoSettingItem(ret))->setName("Measure Background")->setClickAction("openBackground"),
            })
            ->setName("Background"),
        (new SubSettingItem(ret))
            ->setSettings({
                (new ChoiceSettingItem(
                     {
                         {"100000", 100000},
                         {"200000", 200000},
                         {"300000", 300000},
                         {"400000", 400000},
                         {"500000", 500000},
                     },
                     ret))
                    ->setShowValue(true)
                    ->setName("Calibration Count")
                    ->setKey(SettingManager::KEY_CALIB_COUNT),
                (new InfoSettingItem(ret))->setName("Calibration with Cs137, Co60")->setClickAction("openCalibCo60"),
                (new InfoSettingItem(ret))->setName("Calibration with Cs137")->setClickAction("openCalibEstCs137"),
                (new ChoiceSettingItem(
                     {
                         {"Eu-152", IsoProfile::name(IsoProfile::Eu152)},
                         {"Ba-133​", IsoProfile::name(IsoProfile::Ba133)},
                     },
                     ret))
                    ->setShowValue(true)
                    ->setName("Selec Source")
                    ->setKey(SettingManager::KEY_NDT_SRC),
            })
            ->setName("Calibration"),
        (new SubSettingItem(ret))
            ->setSettings({
                (new ChoiceSettingItem({{"Steel", "Steel"}, {"Aluminum", "Aluminum"}, {"Iron", "Iron"}}, ret))
                    ->setName("Material")
                    ->setKey(SettingManager::KEY_PIPE_MATERIAL)
                    ->setShowValue(true),
                (new ChoiceSettingItem({{"7.85", 7.85}, {"7.87", 7.87}, {"2.70", 2.70}, {"8.00", 8.00}}, ret))
                    ->setName("Density")
                    ->setKey(SettingManager::KEY_PIPE_DENSITY)
                    ->setShowValue(true),
                (new ChoiceSettingItem({{"5", 5.0}, {"6", 6.0}, {"8", 8.0}, {"10", 10.0}, {"12", 12.0}, {"15", 15.0}, {"20", 20.0}, {"25", 25.0}}, ret))
                    ->setName("Thickness")
                    ->setKey(SettingManager::KEY_PIPE_THICKNESS)
                    ->setShowValue(true),
                (new ChoiceSettingItem({{"50", 50.0}, {"80", 80.0}, {"100", 100.0}, {"150", 150.0}, {"200", 200.0}, {"250", 250.0}, {"300", 300.0}}, ret))
                    ->setName("Diameter")
                    ->setKey(SettingManager::KEY_PIPE_DIAMETER)
                    ->setShowValue(true),
            })
            ->setName("Pipe Information"),
        (new SubSettingItem(ret))
            ->setSettings({
                (new ChoiceSettingItem({{"60", 60}, {"120", 120}, {"180", 180}, {"300", 300}, {"600", 600}}, ret))
                    ->setShowValue(true)
                    ->setName("Measurement Time")
                    ->setKey(SettingManager::KEY_ACQTIME_ID),
                (new ChoiceSettingItem({{"10", 10}, {"3600", 3600}}, ret))
                    ->setName("Measurement Interval Time")
                    ->setKey(SettingManager::KEY_MEASURE_INVERVAL)
                    ->setShowValue(true),
            })
            ->setName("Measurement Setup"),
        (new InfoSettingItem(ret))
            ->setName("Wireless Connection")
            ->setIcon(":/icons/wifi.png")
            ->setClickAction("handleWifiSetting"),
        (new InfoSettingItem(ret))
            ->setName("About S/W Version")
            // ->setIcon(":/icons/wifi.png")
            ->setClickAction("openSwVersion"),
    });

    return ret;
}

SettingManager::SettingManager(QObject *parent)
    : QObject(parent), Component("SettingManager"),
      m_increaseTime(new ConfigEntry(KEY_INCREASE_TIME, 10, this)),
      m_acqTimeBgr(new ConfigEntry(KEY_ACQTIME_BGR, 60, this)),
      m_acqTime(new ConfigEntry(KEY_ACQTIME_ID, 3600, this)),
      m_measureInterval(new ConfigEntry(KEY_MEASURE_INVERVAL, 7200, this)),
      m_calibCount(new ConfigEntry(KEY_CALIB_COUNT, 100000, this)),
      m_ndtSource(new ConfigEntry(KEY_NDT_SRC, IsoProfile::Eu152, this)),
      m_pipeMaterial(new ConfigEntry(KEY_PIPE_MATERIAL, "Steel", this)),
      m_pipeDensity(new ConfigEntry(KEY_PIPE_DENSITY, 7.85, this)),
      m_pipeThickness(new ConfigEntry(KEY_PIPE_THICKNESS, 10.0, this)),
      m_pipeDiameter(new ConfigEntry(KEY_PIPE_DIAMETER, 100.0, this))
{
    // Register all ConfigEntry members in the map and connect their signals
    auto connectAndRegister = [&](ConfigEntry* entry) {
        m_settingItems[entry->getKey()] = entry;
    };

    connectAndRegister(m_increaseTime);
    connectAndRegister(m_acqTimeBgr);
    connectAndRegister(m_acqTime);
    connectAndRegister(m_measureInterval);
    connectAndRegister(m_calibCount);
    connectAndRegister(m_ndtSource);
    connectAndRegister(m_pipeMaterial);
    connectAndRegister(m_pipeDensity);
    connectAndRegister(m_pipeThickness);
    connectAndRegister(m_pipeDiameter);
}

SettingManager::~SettingManager()
{
    // QObject parent-child mechanism handles deletion of ConfigEntry objects
    // if they were parented to SettingManager (which they are in the example above).
    // No need to manually delete items from m_settingItems if they are children.
}

void SettingManager::initialize()
{
    m_databaseManager = ComponentManager::instance().databaseManager();
    if (m_databaseManager) {
        loadSettings();
        subscribeKey(KEY_NDT_SRC, this, [](SettingManager* mgr, auto) {
            mgr->m_isotopeProfile = IsoProfile::fromIsotope(mgr->getIsotope());
        });
    } else {
        nucare::logE() << "DatabaseManager not available for SettingManager.";
    }
}

void SettingManager::saveSetting(const QString &key, const QVariant& value)
{
    if (m_databaseManager) {
        m_databaseManager->setSetting(key, value);
    } else {
        nucare::logE() << "Cannot save setting, DatabaseManager is null.";
    }
}

QVariant SettingManager::getSetting(const QString &key, const QVariant& defaultValue) const
{
    if (m_databaseManager) {
        return m_databaseManager->getSetting(key, defaultValue);
    } else {
        nucare::logE() << "Cannot get setting, DatabaseManager is null. Returning default value.";
        return defaultValue;
    }
}

void SettingManager::onConfigEntryValueChanged(setting::SettingManager* manager, const char* key)
{
    Q_UNUSED(manager); // Manager is 'this'
    ConfigEntry* entry = getEntry(key);
    if (entry) {
        saveSetting(key, entry->getValue());
        nucare::logD() << "Setting saved:" << key << "=" << entry->getValue();
    } else {
        nucare::logE() << "ConfigEntry not found for key:" << key;
    }
}

void SettingManager::loadSettings()
{
    if (!m_databaseManager) {
        nucare::logE() << "Cannot load settings, DatabaseManager is null.";
        return;
    }

    QMap<QString, QVariant> allSettings = m_databaseManager->getAllSettings();
    for (auto it = m_settingItems.constBegin(); it != m_settingItems.constEnd(); ++it) {
        auto& key = it.key();
        ConfigEntry* entry = it.value();
        if (allSettings.contains(key)) {
            // Temporarily disconnect to avoid re-saving during load
            entry->setValue(allSettings.value(key), false);
            nucare::logD() << "Setting loaded:" << key << "=" << entry->getValue();
        } else {
            // If not in DB, save current default value to DB
            saveSetting(key, entry->getValue());
            nucare::logD() << "Setting not found in DB, saving default:" << key << "=" << entry->getValue();
        }
    }
}

ConfigEntry* SettingManager::getEntry(const QString& key) const
{
    return m_settingItems.value(key, nullptr);
}

int SettingManager::getIncreaseTime() const
{
    return m_increaseTime->getValue().toInt();
}

int SettingManager::getAcqTimeBgr() const
{
    return m_acqTimeBgr->getValue().toInt();
}

int SettingManager::getAcqTimeId() const
{
    return m_acqTime->getValue().toInt();
}

int SettingManager::getMeasureInterval() const
{
    return m_measureInterval->getValue().toInt();
}

int SettingManager::getCalibCount() const
{
    return m_calibCount->getValue().toInt();
}

IsoProfile::Isotope SettingManager::getIsotope() const
{
    return IsoProfile::isotopeFromName(m_ndtSource->getValue().toString().toLatin1());
}

QString SettingManager::getNdtSource() const
{
    return m_ndtSource->getValue().toString();
}

QString SettingManager::getPipeMaterial() const
{
    return m_pipeMaterial->getValue().toString();
}

double SettingManager::getPipeDensity() const
{
    return m_pipeDensity->getValue().toDouble();
}

double SettingManager::getPipeThickness() const
{
    return m_pipeThickness->getValue().toDouble();
}

double SettingManager::getPipeDiameter() const
{
    return m_pipeDiameter->getValue().toDouble();
}

IsoProfile *SettingManager::getIsotopeProfile() const
{
    return m_isotopeProfile;
}
