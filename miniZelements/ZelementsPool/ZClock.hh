#ifndef ZCLOCK_HH
#define ZCLOCK_HH

#include <sys/time.h>
#include <stdint.h>

namespace mfw { namespace zel {

//TODO: !!!!! this calss needs hal too !!!!!!
class ZClock {
public:
    ZClock(void) : m_Offset(0), m_Set(false), m_Speed(100){};
    virtual ~ZClock() {};

    virtual uint64_t GetClock() {
        struct timeval tv;
        gettimeofday(&tv, 0);
        int64_t tmp = tv.tv_sec;
        tmp *= 1000000;
        tmp += tv.tv_usec;
        tmp *= 1000;
        tmp += m_Offset;
        return static_cast<uint64_t>(tmp);//ns
    };
    virtual void SetClock(uint64_t a_Time) {
        struct timeval tv;
        gettimeofday(&tv, 0);
        m_Offset = a_Time - (tv.tv_sec*1000000 + tv.tv_usec) * 1000;
        m_Set = true;
    };
    virtual void SetSpeed(int a_Speed) {
        m_Speed = a_Speed;//yep not implemented
    }

    virtual int GetSpeed(void) {
        return m_Speed;
    }

    bool IsSet() {
        return m_Set;
    }

    virtual void ResetClock() {
        m_Set = false;
        m_Offset = 0;
    }

protected:
    int64_t m_Offset;//TODO I don;t think we need 64 here but... ?
    bool m_Set;
    int m_Speed;//speed in %
};

ZClock* CreateSTC();

};};
#endif
