cmake_minimum_required(VERSION 3.5)

project(NDT VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(util) # Add util directory for header searches
include_directories(widget) # Add widget directory for header searches
include_directories(component) # Add component directory
include_directories(page) # Add page directory
include_directories(page/home) # Add home directory for header searches
include_directories(page/setting) # Add setting directory for header searches
include_directories(page/bgr) # Add bgr directory for header searches
include_directories(page/calibration) # Add calibration directory for header searches
include_directories(page/spectrum_viewer) # Add spectrum_viewer directory for header searches
include_directories(page/event_detail) # Add event_detail directory for header searches
include_directories(${Qt${QT_VERSION_MAJOR}_INCLUDE_DIRS} ${QT_QTCORE_INCLUDE_DIRS})

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Core SerialPort Sql REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Core SerialPort Sql REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        application.cpp
        application.h
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        thememanager.cpp
        thememanager.h
        util/util.h
        util/util.cpp
        util/ndt_util.h
        util/ndt_util.cpp
        util/nc_exception.h
        util/nc_exception.cpp
        util/NcLibrary.h
        util/NcLibrary.cpp
        util/PeakSearch.h
        util/PeakSearch.cpp
        config.h
        base/baseview.h
        base/baseview.cpp
        base/basescreen.h
        base/basescreen.cpp
        base/basewindow.h
        base/basewindow.cpp
        base/basedialog.h
        base/basedialog.cpp
        base/basedialog.ui
        widget/AcqTimeDialog.h
        widget/AcqTimeDialog.cpp
        widget/AcqTimeDialog.ui
        widget/NcButton.h
        widget/NcButton.cpp
        widget/statusbarwidget.h
        widget/statusbarwidget.cpp
        widget/statusbarwidget.ui
        widget/menuwidget.h
        widget/menuwidget.cpp
        widget/menuwidget.ui
        widget/baselist.h
        widget/baselist.cpp
        widget/basemodel.h
        widget/concentriccircleswidget.h
        widget/concentriccircleswidget.cpp
        widget/ChoicesDialog.h
        widget/ChoicesDialog.cpp
        widget/ChoicesDialog.ui
        widget/settingitemdelegate.h
        widget/settingitemdelegate.cpp
        widget/SpectrumView.h
        widget/SpectrumView.cpp
        widget/ShutdownDialog.h
        widget/ShutdownDialog.cpp
        component/component.h
        component/component.cpp
        component/navigationcomponent.h
        component/navigationcomponent.cpp
        component/componentmanager.h
        component/componentmanager.cpp
        component/inputcomponent.h
        component/inputcomponent.cpp
        component/detectorcomponent.h
        component/detectorcomponent.cpp
        component/databasemanager.h
        component/databasemanager.cpp
        component/settingmanager.h
        component/settingmanager.cpp
        component/ncmanager.h
        component/ncmanager.cpp
        component/gamma_ndt.h
        component/gamma_ndt.c
        component/SpectrumAccumulator.cpp
        component/SpectrumAccumulator.h
        model/Background.h
        model/basesettingitem.h
        model/basesettingitem.cpp
        model/Calibration.h
        model/Calibration.cpp
        model/DetectorCode.h
        model/DetectorCode.cpp
        model/DetectorInfo.h
        model/DetectorProp.cpp
        model/DetectorProp.h
        model/DetectorCalibConfig.h
        model/Event.h
        model/Event.cpp
        model/Matrix.h
        model/Matrix.cpp
        model/Material.h
        model/ndt_model.h
        model/ndt_model.cpp
        model/settingmodel.h
        model/settingmodel.cpp
        model/DetectorModels.h
        model/Types.h
        model/Time.h
        model/Spectrum.h
        model/DetectorProp.h
        model/DetectorProp.cpp
        page/home/homepage.h
        page/home/homepage.cpp
        page/home/homepage.ui
        page/setting/settingscreen.h
        page/setting/settingscreen.cpp
        page/bgr/BgrScreen.ui
        page/bgr/BgrScreen.h
        page/bgr/BgrScreen.cpp
        page/spectrum_viewer/SpectrumViewerScreen.ui
        page/spectrum_viewer/SpectrumViewerScreen.h
        page/spectrum_viewer/SpectrumViewerScreen.cpp
        page/calibration/CalibrationScreen.ui
        page/calibration/CalibrationScreen.h
        page/calibration/CalibrationScreen.cpp
        page/calibration/CalibAcqDialog.h
        page/calibration/CalibAcqDialog.cpp
        page/events/EventListScreen.h
        page/events/EventListScreen.cpp
        page/events/EventListScreen.ui
        page/events/EventListAdapter.h
        page/events/EventListAdapter.cpp
        page/event_detail/EventDetailScreen.h
        page/event_detail/EventDetailScreen.cpp
        page/event_detail/EventDetailScreen.ui
        page/event_detail/EventDetailArgs.h
        page/swinfo/SwVersion.ui
        page/swinfo/SwVersionDialog.h
        page/swinfo/SwVersionDialog.cpp
        controller/wifi_service.h
        controller/wifi_service.cpp
        controller/platform_controller.h
        controller/platform_controller.cpp
        res/resource.qrc
        util/dualsocketnotifier.h
        util/dualsocketnotifier.cpp
)

set_source_files_properties(component/gamma_ndt.c PROPERTIES LANGUAGE C)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(NDT
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET NDT APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(NDT SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(NDT
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(NDT PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::Sql wpa_client)

set_target_properties(NDT PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(NDT)
endif()

install(TARGETS NDT RUNTIME DESTINATION /usr/bin)
install(FILES res/NDT.db DESTINATION ${CMAKE_INSTALL_PREFIX}/share/NDT)
