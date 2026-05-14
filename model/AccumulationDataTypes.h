#ifndef ACCUMULATIONDATATYPES_H
#define ACCUMULATIONDATATYPES_H

#include "model/Spectrum.h" // For Spectrum and HwSpectrum
#include <QDateTime>        // For QDateTime
#include <memory>           // For std::shared_ptr
#include <limits>           // For std::numeric_limits
// #include <variant>       // REMOVE THIS LINE

// Define ActiveSpectrumType enum (can be class enum if preferred, but regular enum is fine too)
// Let's use enum class for better scoping, as it was in the plan.
enum class ActiveSpectrumType {
    None,
    TypeSpectrum,    // Indicates Spectrum (float, 1024) is active
    TypeHwSpectrum   // Indicates HwSpectrum (int, 2048) is active
};

// AccumulatorState enum remains the same (simplified version)
enum class AccumulatorState {
    Idle,
    Waiting,    // Configured, ready to start (manual/continuous trigger)
    Measuring,
    Completed   // Cycle finished, final result emitted, before transitioning to Idle or Waiting (continuous)
};

enum class AccumulationMode { // New enum class
    NotSet,
    ByCount,
    ByTime,
    ContinuousByCount,
    ContinuousByTime
};

struct AccumulationResult {
    ActiveSpectrumType activeType;
    std::shared_ptr<Spectrum> spectrum;     // Valid if activeType is TypeSpectrum
    std::shared_ptr<HwSpectrum> hwSpectrum; // Valid if activeType is TypeHwSpectrum
    QDateTime startTime;
    QDateTime finishTime;
    double cps;
    double executionRealtimeSeconds;
    uint count;
    double avgCPS = 0;
    double maxCPS = 0;
    double minCPS = std::numeric_limits<double>::max();

    qlonglong detectorId;
    qlonglong backgroundId;
    qlonglong calibrationId;

    // Constructor to initialize type and clear pointers
    AccumulationResult() : activeType(ActiveSpectrumType::None),
        spectrum(nullptr), hwSpectrum(nullptr), count(0), cps(0.0),
        executionRealtimeSeconds(0.0), avgCPS(0.0), maxCPS(0.0),
        minCPS(std::numeric_limits<double>::max()),
        detectorId(-1), backgroundId(-1), calibrationId(-1) {}
};

#endif // ACCUMULATIONDATATYPES_H
