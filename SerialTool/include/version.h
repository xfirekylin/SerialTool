#ifndef __VERSION_H
#define __VERSION_H

#define MAIN_VERSION      1.2.5

#define SOFTWARE_NAME     "SerialTool"
#define COPYRIGHT         "Copyleft  2017-2018 wwtiot"

#define _STR_(s)           #s
#define __STR(s)          _STR_(s)

#define BUILD_VERSION     _STR_(48b44)
#define SOFTWARE_VERSION  __STR(MAIN_VERSION)

#endif
