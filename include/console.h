/*
 * PROJECT:     FreeLoader wrapper for Apple TV
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for screen printing functions for the original Apple TV
 * COPYRIGHT:   Copyright 2023-2024 DistroHopper39B (distrohopper39b.business@gmail.com)
 */

#include <stdarg.h>
#ifndef _CONSOLE_H
#define _CONSOLE_H

extern void ClearScreen(bool VerboseEnable);
extern void SetupScreen();
extern void printf(const char *szFormat, ...);
extern void ChangeColors(u32 Foreground, u32 Background);
extern int vsprintf(char *buf, const char *fmt, va_list args);
extern bool WrapperVerbose;

typedef enum {
    Blue = 0,
    Green,
    Red,
    Reserved,
} FrameBufferColors;

#define COM1 0x3F8

#define debug_printf(...)   if (WrapperVerbose) \
                            printf(__VA_ARGS__)

#define trace(...)          debug_printf("(%s:%d) TRACE: ", __FILE__, __LINE__); \
                            debug_printf(__VA_ARGS__)
#define warn(...)           printf("(%s:%d) WARNING: ", __FILE__, __LINE__); \
                            printf(__VA_ARGS__)
#define error(...)          printf("(%s:%d) ERROR: ", __FILE__, __LINE__); \
                            printf(__VA_ARGS__)
#define fatal(...)          printf("(%s:%d) FATAL: ", __FILE__, __LINE__); \
                            printf(__VA_ARGS__); \
                            fail()
#endif //_CONSOLE_H
