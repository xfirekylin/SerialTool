#-------------------------------------------------
#
# Project created by QtCreator 2017-02-01T17:03:23
#
#-------------------------------------------------
QT       += core gui widgets serialport network charts
QT       -= console

TARGET = SerialTool

TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS QSCINTILLA_DLL

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

unix {
    QMAKE_LFLAGS += -fno-pie -no-pie # create executable file
}

TRANSLATIONS += language/serialtool_zh_CN.ts

RC_FILE += resource/serialtool.rc

INCLUDEPATH += include

SOURCES += \
    source/aboutbox.cpp \
    source/channelitem.cpp \
    source/main.cpp \
    source/optionsbox.cpp \
    source/portsetbox.cpp \
    source/textedit.cpp \
    source/wavedecode.cpp \
    source/oscilloscope.cpp \
    source/filethread.cpp \
    source/xmodem.cpp \
    source/vediobox.cpp \
    source/tcpudpport.cpp \
    source/defaultconfig.cpp \
    source/oscopetimestamp.cpp \
    source/terminalview.cpp \
    source/serialport.cpp \
    source/pointdatabuffer.cpp \
    source/valuedisplay.cpp \
    source/mainwindow.cpp \
    source/filetransmitview.cpp

HEADERS  += \
    include/aboutbox.h \
    include/channelitem.h \
    include/optionsbox.h \
    include/portsetbox.h \
    include/textedit.h \
    include/version.h \
    include/wavedecode.h \
    include/oscilloscope.h \
    include/filethread.h \
    include/xmodem.h \
    include/xmodem_crc16.h \
    include/vediobox.h \
    include/tcpudpport.h \
    include/defaultconfig.h \
    include/oscopetimestamp.h \
    include/terminalview.h \
    include/serialport.h \
    include/pointdatabuffer.h \
    include/valuedisplay.h \
    include/mainwindow.h \
    include/filetransmitview.h

DISTFILES += \
    resource/images/clear.png \
    resource/images/close.png \
    resource/images/connect.png \
    resource/images/port config.png \
    resource/images/config.ico \
    resource/images/exit.ico \
    resource/images/icon.ico \
    resource/images/pause.ico \
    resource/images/save.ico \
    resource/images/start.ico

RESOURCES += \
    resource/serialtool.qrc

FORMS += \
    ui/aboutbox.ui \
    ui/optionsbox.ui \
    ui/portsetbox.ui \
    ui/oscilloscope.ui \
    ui/vediobox.ui \
    ui/tcpudpport.ui \
    ui/terminalview.ui \
    ui/valuedisplay.ui \
    ui/serialport.ui \
    ui/mainwindow.ui \
    ui/filetransmitview.ui



win32:CONFIG(release, debug|release): LIBS += -LD:/Qt/Qt5.12.12/5.12.12/mingw73_64/lib/ -lqscintilla2_qt5
else:win32:CONFIG(debug, debug|release): LIBS += -LD:/Qt/Qt5.12.12/5.12.12/mingw73_64/lib/ -lqscintilla2_qt5d
else:unix: LIBS += -LD:/Qt/Qt5.12.12/5.12.12/mingw73_64/lib/ -lqscintilla2_qt5

INCLUDEPATH += D:/Qt/Qt5.12.12/5.12.12/mingw73_64/include
DEPENDPATH += D:/Qt/Qt5.12.12/5.12.12/mingw73_64/include
