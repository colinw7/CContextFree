TEMPLATE = app

TARGET = CContextFree

DEPENDPATH += .

QT += widgets

CONFIG += debug

# Input
SOURCES += \
CContextFree.cpp \
CContextFreeEval.cpp \
CQContextFree.cpp \

HEADERS += \
CContextFreeEval.h \
CContextFree.h \
CQContextFree.h \

DESTDIR     = ../bin
OBJECTS_DIR = ../obj
LIB_DIR     = ../lib

INCLUDEPATH += \
../include \
../../CFile/include \
../../CMath/include \
../../CStrUtil/include \
../../COS/include \
../../CMath/include \
../../CUtil/include \
.

unix:LIBS += \
-L$$LIB_DIR \
-L../../CFile/lib \
-L../../CMath/lib \
-L../../CStrUtil/lib \
-L../../COS/lib \
-lCFile \
-lCOS \
-lCStrUtil \
