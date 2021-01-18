TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
target.path=/home/sxx
INSTALLS += target
SOURCES += main.c \
    tl-parser.c \
    tl-logger.c \
    tl-canbus.c \
    tl-gps.c \
    tl-net.c \
    jtt808.c

HEADERS += \
    tl-parser.h \
    tl-logger.h \
    tl-canbus.h \
    tl-gps.h \
    tl-net.h \
    jtt808.h

unix:!macx: LIBS += -L$$PWD/../../lib/glib/lib/ -lglib-2.0 -lgio-2.0 -lgobject-2.0

INCLUDEPATH += $$PWD/../../lib/glib/include/glib-2.0
INCLUDEPATH += $$PWD/../../lib/glib/lib/glib-2.0/include
#DEPENDPATH += $$PWD/../../lib/glib/include

unix:!macx: LIBS += -L$$PWD/../../lib/json-c/lib/ -ljson-c


INCLUDEPATH += $$PWD/../../lib/json-c/include/json-c
#DEPENDPATH += $$PWD/../../lib/json-c/include

unix:!macx: LIBS += -L$$PWD/../../lib/libffi/lib/ -lffi

INCLUDEPATH += $$PWD/../../lib/libffi/include
DEPENDPATH += $$PWD/../../lib/libffi/include

#unix:!macx: LIBS += -L$$PWD/../../lib/gpsd -lgps
#INCLUDEPATH += $$PWD/../../lib/gpsd
