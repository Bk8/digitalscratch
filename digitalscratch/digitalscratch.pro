# Release version number.
#VERSION = 1.5.0

# Snapshot version number.
win32 {
    VERSION = 1.6.0b
}
unix {
    CURRENT_DATE = $$system(date +%Y%m%d)
    VERSION = 1.5.0+1.6.0SNAPSHOT$${CURRENT_DATE}
}
DEFINES += VERSION=$${VERSION}

##############################
# Paths
unix {
    BIN_PATH = $${PREFIX}/bin
}
win32 {
    BIN_PATH = .
}

# Installation paths and files.
target.path = $${BIN_PATH}
INSTALLS += target
##############################

TEMPLATE = app

contains(QT_VERSION, ^4\\.[0-9]\\..*) {
    #Qt 4.x
    QT += gui sql
}
else {
    # Qt 5 and more
    QT += gui widgets sql concurrent multimedia
}

DEFINES += ENABLE_TEST_DEVICE
CONFIG(test-no_device_test) {
    CONFIG   += test
    DEFINES  -= ENABLE_TEST_DEVICE
}
CONFIG(test) {
    QT       += testlib
    TARGET    = digitalscratch-test
    CONFIG   += console
    CONFIG   -= app_bundle
    DEFINES  += ENABLE_TEST_MODE
}
else {
    DEFINES  -= ENABLE_TEST_DEVICE
    TARGET    = digitalscratch
}

DEPENDPATH += . src include/gui include/player src/gui src/player
INCLUDEPATH += . include/player include/gui include


# Input
HEADERS += include/gui/config_dialog.h \
           include/gui/gui.h \
           include/gui/waveform.h \
           include/gui/remaining_time.h \
           include/gui/application_settings.h \
           include/player/audio_file_decoding_process.h \
           include/player/audio_track.h \
           include/player/audio_track_playback_process.h \
           include/player/sound_driver_access_rules.h \
           include/player/jack_access_rules.h \
           include/player/audio_device_access_rules.h \
           include/player/timecode_control_process.h \
           include/player/manual_control_process.h \
           include/player/playback_parameters.h \
           include/player/sound_capture_and_playback_process.h \
           include/player/data_persistence.h \
           include/player/audio_collection_model.h \
           include/player/audio_track_key_process.h \
           include/player/playlist.h \
           include/player/playlist_persistence.h \
           include/utils.h \
           include/application_const.h \
           include/singleton.h \
           include/application_logging.h
           
SOURCES += src/main.cpp \
           src/gui/config_dialog.cpp \
           src/gui/gui.cpp \
           src/gui/waveform.cpp \
           src/gui/remaining_time.cpp \
           src/gui/application_settings.cpp \
           src/player/audio_file_decoding_process.cpp \
           src/player/audio_track.cpp \
           src/player/audio_track_playback_process.cpp \
           src/player/sound_driver_access_rules.cpp \
           src/player/jack_access_rules.cpp \
           src/player/audio_device_access_rules.cpp \
           src/player/timecode_control_process.cpp \
           src/player/manual_control_process.cpp \
           src/player/playback_parameters.cpp \
           src/player/sound_capture_and_playback_process.cpp \
           src/player/data_persistence.cpp \
           src/player/audio_collection_model.cpp \
           src/player/audio_track_key_process.cpp \
           src/player/playlist.cpp \
           src/player/playlist_persistence.cpp \
           src/utils.cpp \
           src/application_logging.cpp

CONFIG(test) {
    INCLUDEPATH += test

    SOURCES -= src/main.cpp

    HEADERS += test/audio_track_test.h \
               test/audio_file_decoding_process_test.h \
               test/utils_test.h \
               test/data_persistence_test.h \
               test/playlist_persistence_test.h \
               test/audio_device_access_rules_test.h \
               test/sound_capture_and_playback_process_test.h

    SOURCES += test/main_test.cpp \
               test/audio_track_test.cpp \
               test/audio_file_decoding_process_test.cpp \
               test/utils_test.cpp \
               test/data_persistence_test.cpp \
               test/playlist_persistence_test.cpp \
               test/audio_device_access_rules_test.cpp \
               test/sound_capture_and_playback_process_test.cpp
}


#############################
# LibAV / FFmpeg
win32 {
    LIBS += -L$$PWD/win-external/ffmpeg/lib/ -lavformat -lavcodec -lavutil
    INCLUDEPATH += $$PWD/win-external/ffmpeg/include
    DEPENDPATH += $$PWD/win-external/ffmpeg/include
}
unix {
    LIBS += -lavformat -lavcodec -lavutil
}
#############################

#############################
# DigitalScratch library
win32 {
    LIBS += -L$$PWD/win-external/libdigitalscratch/lib/ -ldigitalscratch1
    INCLUDEPATH += $$PWD/win-external/libdigitalscratch/include
    DEPENDPATH += $$PWD/win-external/libdigitalscratch/include
}
unix {
    LIBS += -ldigitalscratch
}
#############################

#############################
# Lib sample rate
win32 {
    LIBS += -L$$PWD/win-external/samplerate-0.1.8/lib/ -llibsamplerate-0
    INCLUDEPATH += $$PWD/win-external/samplerate-0.1.8/include
    DEPENDPATH += $$PWD/win-external/samplerate-0.1.8/include
}
unix {
    LIBS += -lsamplerate
}
#############################

#############################
# Jack lib
DEFINES += USE_JACK
win32 {
    LIBS += -L$$PWD/win-external/jack-1.9.9/lib/ -llibjack
    INCLUDEPATH += $$PWD/win-external/jack-1.9.9/includes
    DEPENDPATH += $$PWD/win-external/jack-1.9.9/includes
}
unix {
    LIBS += -ljack -lpthread -lrt
}
#############################

#############################
# lib KeyFinder
win32 {
    LIBS += -L$$PWD/win-external/libkeyfinder/lib/ -lkeyfinder0
    INCLUDEPATH += $$PWD/win-external/libkeyfinder/include
    DEPENDPATH += $$PWD/win-external/libkeyfinder/include
}
unix {
    LIBS += -lkeyfinder
}

#############################
# Enable for release and debug mode.
CONFIG += debug_and_release
#############################

#############################
# Enable multi-threading support for QT
CONFIG += qt thread c++11
#############################

############################
# Icon for Windows build
win32:RC_FILE = digitalscratch_resource.rc
############################

############################
# Copy necessary files to run on windows
win32 {
    DLLS = \
        $${PWD}/win-external/ffmpeg/lib/avcodec-55.dll \
        $${PWD}/win-external/ffmpeg/lib/avutil-52.dll \
        $${PWD}/win-external/ffmpeg/lib/avformat-55.dll \
        $${PWD}/win-external/libdigitalscratch/lib/digitalscratch1.dll \
        $${PWD}/win-external/samplerate-0.1.8/lib/libsamplerate-0.dll \
        $${PWD}/win-external/jack-1.9.9/lib/libjack.dll \
        $${PWD}/win-external/libkeyfinder/lib/keyfinder0.dll \
        $${PWD}/win-external/libkeyfinder/lib/libfftw3-3.dll
    DESTDIR_WIN = $${DESTDIR}
    CONFIG(debug, debug|release) {
        DESTDIR_WIN += debug
        DESTDIR_PLATFORM_WIN += debug/platforms
        DLLS += %QTDIR%/bin/Qt5Cored.dll \
                %QTDIR%/bin/Qt5Guid.dll \
                %QTDIR%/bin/Qt5Widgetsd.dll \
                %QTDIR%/bin/Qt5Concurrentd.dll \
                %QTDIR%/bin/icu*.dll \
                %QTDIR%/bin/Qt5Sqld.dll
        DLLS_PLATFORMS = %QTDIR%/plugins/platforms/qwindowsd.dll
        CONFIG(test) {
           DLLS += %QTDIR%/bin/Qt5Testd.dll
        }
    } else {
        DESTDIR_WIN += release
        DESTDIR_PLATFORM_WIN += release/platforms
        DLLS += %QTDIR%/bin/Qt5Core.dll \
                %QTDIR%/bin/Qt5Gui.dll \
                %QTDIR%/bin/Qt5Widgets.dll \
                %QTDIR%/bin/Qt5Concurrent.dll \
                %QTDIR%/bin/icu*.dll \
                %QTDIR%/bin/Qt5Sql.dll
        DLLS_PLATFORMS = %QTDIR%/plugins/platforms/qwindows.dll
        CONFIG(test) {
           DLLS += %QTDIR%/bin/Qt5Test.dll
        }
    }
    DLLS ~= s,/,\\,g
    DLLS_PLATFORMS ~= s,/,\\,g
    DESTDIR_WIN ~= s,/,\\,g
    DESTDIR_PLATFORM_WIN ~= s,/,\\,g
    for(FILE, DLLS){
        QMAKE_POST_LINK += $${QMAKE_COPY} $$quote($${FILE}) $$quote($${DESTDIR_WIN}) $$escape_expand(\\n\\t)
    }
    QMAKE_POST_LINK += if not exist $$quote($${DESTDIR_PLATFORM_WIN}) $${QMAKE_MKDIR} $$quote($${DESTDIR_PLATFORM_WIN}) $$escape_expand(\\n\\t)
    for(FILE, DLLS_PLATFORMS){
        QMAKE_POST_LINK += $${QMAKE_COPY} $$quote($${FILE}) $$quote($${DESTDIR_PLATFORM_WIN}) $$escape_expand(\\n\\t)
    }

    # Copy test data into .exe directory
    CONFIG(test) {
        SRC_TESTDATA_DIR = test/data
        SRC_TESTDATA_DIR ~= s,/,\\,g
        DEST_TESTDATA_DIR = $${DESTDIR_WIN}/data
        DEST_TESTDATA_DIR ~= s,/,\\,g
        QMAKE_POST_LINK += if not exist $$quote($${DEST_TESTDATA_DIR}) $${QMAKE_MKDIR} $$quote($${DEST_TESTDATA_DIR}) $$escape_expand(\\n\\t)
        QMAKE_POST_LINK += $${QMAKE_COPY} $$quote($${SRC_TESTDATA_DIR}) $$quote($${DEST_TESTDATA_DIR}) $$escape_expand(\\n\\t)
    }
}
############################

OTHER_FILES += \
    README \
    NEWS \
    INSTALL \
    COPYING \
    AUTHORS \
    dist/ubuntu/generate_digitalscratch_deb.sh \
    dist/ubuntu/debian/changelog \
    dist/ubuntu/debian/compat \
    dist/ubuntu/debian/control \
    dist/ubuntu/debian/copyright \
    dist/ubuntu/debian/digitalscratch.desktop \
    dist/ubuntu/debian/digitalscratch.install \
    dist/ubuntu/debian/docs \
    dist/ubuntu/debian/rules \
    dist/debian/generate_digitalscratch_deb.sh \
    dist/debian/debian/changelog \
    dist/debian/debian/compat \
    dist/debian/debian/control \
    dist/debian/debian/copyright \
    dist/debian/debian/digitalscratch.desktop \
    dist/debian/debian/digitalscratch.install \
    dist/debian/debian/docs \
    dist/debian/debian/rules \
    skins/dark.css

RESOURCES += \
    digitalscratch.qrc
