#ifndef KPRINT_H
#define KPRINT_H
#include <core/ktypes.h>

#define TEXT_COLOR_WHITE    0xffffffff
#define TEXT_COLOR_BLACK    0xff000000
#define TEXT_COLOR_RED      0xffff0000
#define TEXT_COLOR_GREEN    0xff00ff00
#define TEXT_COLOR_BLUE     0xff0000ff
#define TEXT_COLOR_YELLOW   0xffffff00
#define TEXT_COLOR_COOL     0xff05ff74

#define DEFAULT_TEXT_COLOR  TEXT_COLOR_COOL

void kprintSetCursorLocation(uint32_t x, uint32_t y);

void kprintCharColored(
    char chr,
	unsigned int color
);

void kprintChar(
    char chr
);

void kprintColoredEx(
    const char* str,
    uint32_t color
);

void kprintFmtColored(
    uint32_t color,
    const char* fmt,
    ...
);

void kprint(
    const char* fmt,
    ...
);

void kprintError(
    const char* fmt,
    ...
);

void kprintWarn(
    const char* fmt,
    ...
);

void kprintInfo(
    const char* fmt,
    ...
);

#endif
