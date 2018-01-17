#ifndef ZTYPES_HH
#define ZTYPES_HH
#include <string>

namespace mfw { namespace zel {

typedef int ErrCode_t;
typedef std::string URL; 
enum State {
    ZERO,
    READY,
    PLAYING,
    PAUSED
};

enum RCode {
    OK,
    NOK
};

enum DataType {
    ANY,
    MPEG_TS,
    MPEG_SECTION,
    MPEG_PES,
    MPEG_ES,
    AUDIO,
    AUDIO_MPEG2,
    AUDIO_AAC,
    AUDIO_AAC_ADTS,
    AUDIO_AAC_LATM,
    AUDIO_AC3,
    VIDEO,
    VIDEO_MPEG2,
    VIDEO_MPEG4P2,
    VIDEO_H264,
    TTX,
    DVBSUBT
};

enum Dir {
    IN,
    OUT
};

enum OutputType {
    OUTPUT_SCART        = 1 << 0, /**< SCART */
    OUTPUT_HDMI         = 1 << 1, /**< HDMI */
    OUTPUT_COMPOSITE    = 1 << 2, /**< Composite */
    OUTPUT_SPDIF        = 1 << 3, /**< SPDIF */
    OUTPUT_COMPONENT    = 1 << 4, /**< Component */
    OUTPUT_SVIDEO       = 1 << 5, /**< SVideo */
    OUTPUT_RF           = 1 << 6, /**< RF */
    OUTPUT_ANALOG_AUDIO = 1 << 7  /**< Analog Audio */
};

enum FCCType {
    AUTO                =(('A' << 0) | ('U' << 8) | ('T' << 16) | ('O' << 24)),
    YUYV                =(('Y' << 0) | ('U' << 8) | ('Y' << 16) | ('V' << 24)),
    YUY2                =(('Y' << 0) | ('U' << 8) | ('Y' << 16) | ('2' << 24)),
    YVYU                =(('Y' << 0) | ('V' << 8) | ('Y' << 16) | ('U' << 24)),
    YV12                =(('Y' << 0) | ('V' << 8) | ('1' << 16) | ('2' << 24)),
    BGRA                =(('B' << 0) | ('G' << 8) | ('R' << 16) | ('A' << 24))
};

};};
#endif
