TEMPLATE = app

TARGET = CQContextFree

DEPENDPATH += .

QT += widgets

CONFIG += debug

# Input
SOURCES += \
CQContextFreeTest.cpp \

HEADERS += \
CQContextFreeTest.h \

DESTDIR     = ../bin
OBJECTS_DIR = ../obj
LIB_DIR     = ../lib

INCLUDEPATH += \
../include \
../../CUtil/include \
../../CMath/include \
../../COS/include \
.

unix:LIBS += \
-L$$LIB_DIR \
-L../../CFile/lib \
-L../../CMath/lib \
-L../../CStrUtil/lib \
-L../../COS/lib \
-lCContextFree \
-lCFile \
-lCMath \
-lCOS \
-lCStrUtil \
