
#ifndef IZELEMENT_HH
#define IZELEMENT_HH

#include <string>
#include <queue>
#include <vector>
#include <iostream>
#include <limits>

#include <pthread.h>
#include <semaphore.h>
#include <inttypes.h>
#include <zinc/zos.hh>
#include <zinc/evt.hh>

#include "ZClock.hh"
#include "Pad.hh"

namespace mfw { namespace zel {
#ifdef ZSYS_SOC_API_INTELSMD
const int ZELEMENT_TASK_STACK_SIZE = 1*1024*1024;//smaller stack, else exhausting VM
#else
const int ZELEMENT_TASK_STACK_SIZE = 8*1024*1024;//on ubuntu 64 I have seen over 4 MB so 8 should be safe
#endif

const uint64_t NO_TIME = std::numeric_limits<uint64_t>::max();
const int16_t  NO_RATE = std::numeric_limits<int16_t>::max();

// bytes/sec rate, used by default GetPosTime(), SetPosTime(), GetSizeTime().
// a source zelement implementation should override these, or calcultae a better m_ByteRate
const uint32_t DEFAULT_BYTE_RATE = 1000000;

/*!
* \brief All zelements derive from this base class.
*
*        It assures basick zelements properties:state, events, pads, ...
*/
class IZelement : public os::Task, public evt::EventGenerator {
public:
    IZelement ( ) : os::Task(os::Task::PRIO_7, ZELEMENT_TASK_STACK_SIZE, "zelement"), 
                    m_Name("null"), m_State(ZERO), m_Size(0), m_Position(0), 
                    m_ByteRate(DEFAULT_BYTE_RATE), m_Clock(NULL), m_ClockBase(0), m_Speed(100) {
        pthread_mutex_init(&m_Lock, NULL);
        sem_init(&m_Signal, 0, 0);
    };

    virtual ~IZelement ( ) {
        Kick(new StateEvt(ZERO));   //kick Body()
        Wait(this);    //and make sure it returns
        for (;!m_Cmds.empty();){
            delete m_Cmds.front();
            m_Cmds.pop();
        }

        for (Pad::PadItr i = m_InputsPads.begin(); i != m_InputsPads.end(); ++i) {
            delete *i;
        }
        for (Pad::PadItr i = m_OutputsPads.begin(); i != m_OutputsPads.end(); ++i) {
            delete *i;
        }
        pthread_mutex_destroy(&m_Lock);
    };

    /*!
    * \brief Gives you generic pad.
    *
    *    Call from a zelemnt::GetPad or override if you need more custom pad
    */
    virtual Pad* GetPad(Dir a_Dir) {
        SmartLock mylock(this);

        Pad* pad = new Pad();
        if (a_Dir == IN){
            m_InputsPads.push_back(dynamic_cast<Pad*>(pad));
        }else{
            m_OutputsPads.push_back(dynamic_cast<Pad*>(pad));
        }

        return pad;
    };

    /*
    * brief Removes pad. Exact oposition of GetPad()
    *
    * it's mendatory in case of demuxers | at least extreamly useful
    */
    virtual void RemPad(Pad *a_Pad) {
        SmartLock mylock(this);

        Pad::PadItr i = std::find(m_OutputsPads.begin(), m_OutputsPads.end(), a_Pad);
        if (i != m_OutputsPads.end()) {
            m_OutputsPads.erase(i + 0);
            delete a_Pad;
        } else {
            // check if input, NOTE: bad things will happen if still in use (connected)
            i = std::find(m_InputsPads.begin(), m_InputsPads.end(), a_Pad);
            if (i != m_InputsPads.end()) {
                m_InputsPads.erase(i + 0);
                delete a_Pad;
            } else {
                LOG_ERROR(("Trying to remove unknown pad:%p", a_Pad));
            }
        }
    }

    /* brief verifies given pad exists. No Lock here, pls!!!
     * ret true if pad exists
     *
    */
    virtual bool VerifyPadExists(Pad *a_Pad) {
        if (std::find(m_OutputsPads.begin(), m_OutputsPads.end(), a_Pad) != m_OutputsPads.end() ||
            std::find(m_InputsPads.begin(), m_InputsPads.end(), a_Pad) != m_InputsPads.end()
        ) {
            return true;
        } else {
            return false;
        }

    }

    /*!
    * \brief send event with current pad buffer level
    *
    */
    virtual void CheckAndSendPlaybackDscrEvt(void) {

    }

    /*!
    * \brief Used by SetState to change zelement state
    *
    */
    class StateEvt: public evt::Event {
        public:
            StateEvt(State a_State) : m_State(a_State) {

            };
            virtual ~StateEvt() {};
            State m_State;
    };

    class StateChangedEvt: public evt::Event {
        public:
            StateChangedEvt(IZelement *a_Originator, State a_State) : m_Originator(a_Originator), m_State(a_State) {

            };
            virtual ~StateChangedEvt() {};
            IZelement *m_Originator;
            State m_State;
    };
    
    /*! 
    * \brief Used to indicate Beginning Of Stream
    *
    */
    class BOSEvt: public evt::Event {
        public:
            BOSEvt(int a_A) : a(a_A) {
            };
            virtual ~BOSEvt() {/*EMPTY*/};
            int a;//it's just for fun
    };

    /*!
    * \brief Used to indicate End Of Stream
    *
    */
    class EOSEvt: public evt::Event {
        public:
            EOSEvt(int a_A) : a(a_A) {
            };
            virtual ~EOSEvt() {/*EMPTY*/};
            int a;//it's just for fun
    };

    /*!
    * \brief Used to indicate an error
    *
    *    Can be used as lite verion of exception
    */
    class ErrEvt: public evt::Event {
        public:
            ErrEvt(const char *a_ErrorMsg, int a_ErrorCode = 0, IZelement* a_Who = NULL) : m_Msg(a_ErrorMsg), m_ErrorCode(a_ErrorCode), m_Who(a_Who) {
            };
            virtual ~ErrEvt() {/*EMPTY*/};
            std::string m_Msg;//error mesage
            int m_ErrorCode;
            IZelement *m_Who;
    };

   /*!
    * \brief Used to carry time stamp
    *
    */
    class TimeStEvt: public evt::Event {
        public:
            TimeStEvt(uint64_t a_TimeStamp) : m_TimeStamp(a_TimeStamp) {
            };
            virtual ~TimeStEvt() {/*EMPTY*/};
            uint64_t m_TimeStamp;
    };

   /*!
    * \brief Used to carry new-segment
    *
    */
    class NewSegmentEvt: public evt::Event {
    public:
        NewSegmentEvt(uint64_t a_Start = NO_TIME, uint64_t a_Stop = NO_TIME,
                      uint64_t a_LinearStart = 0, uint64_t a_SegmentPos = 0,
                      int16_t  a_Rate = NO_RATE, int16_t a_AppliedRate = NO_RATE) :
            m_Start(a_Start), m_Stop(a_Stop),
            m_LinearStart(a_LinearStart), m_SegmentPos(a_SegmentPos),
            m_Rate(a_Rate), m_AppliedRate(a_AppliedRate)
        { };
        virtual ~NewSegmentEvt() {/*EMPTY*/};

        uint64_t m_Start;       // (ms) first point in segment to be presented (if forward pb)
        uint64_t m_Stop;        // (ms) last point in segment to be presented (if forward pb)
        uint64_t m_LinearStart; // (ms) Local/play time corresponding to start/stop (dep on direction)
        uint64_t m_SegmentPos;  // (ms) The segment's position in the stream (0..duration)
        int16_t m_Rate;         // (%) Overall play rate required for the segment. NO_RATE if not valid
        int16_t m_AppliedRate;  // (%) Play rate achieved for this segment, by earlier elements in pipeline
    };

    /*! 
    * \brief Used to flush a Zelement, release all buffers etc.
    * Should be passed on downstream to flush rest of pipeline.
    *
    */
    class FlushEvt: public evt::Event {
        public:
            FlushEvt() {/*EMPTY*/  };
            virtual ~FlushEvt() {/*EMPTY*/};
    };

    /*!
    * \brief USed between pads. Tells other pad to prepera for disconnection
    *
    * It can be used in many other scenarios then pad disconnection too
    */
    class DisconnectEvt: public evt::Event {
        public:
            DisconnectEvt() { /*EMPTY*/ };
            virtual ~DisconnectEvt() {/*EMPTY*/};
    };

    /*!
    * \brief Extends TimeStEvt to audio descriptor
    *
    */
    class AudDscrEvt: public TimeStEvt{
        public:
            AudDscrEvt(uint64_t a_TimeStamp, int a_Rate, int a_Ch, int a_Depth)
                : TimeStEvt(a_TimeStamp),
                m_Rate(a_Rate),
                m_Ch(a_Ch),
                m_Depth(a_Depth)
            {
            };
            virtual ~AudDscrEvt() {/*EMPTY*/};
            int m_Rate;
            int m_Ch;
            int m_Depth;
    };

    /*!
    * \brief Extends TimeStEvt to video raw frame descriptor
    *
    */
    class VidDscrEvt: public TimeStEvt{
        public:
            VidDscrEvt(uint64_t a_TimeStamp, int a_Width, int a_Height, int a_ColorSpace)
                : TimeStEvt(a_TimeStamp),
                m_Height(a_Height),
                m_Width(a_Width),
                m_ColorSpace(a_ColorSpace)
            {
            };
            virtual ~VidDscrEvt() {/*EMPTY*/};
            int m_Height;
            int m_Width;
            int m_ColorSpace;
    };

    /*!
    * \brief Signals new pmt to mfw::I* implementation
    *
    *   TODO:In the end it will be replaced with existing zids section monitor
    */
    class PMTEvt: public evt::Event {
        public:
            PMTEvt() : m_APid(0x1fff),
                       m_VPid(0x1fff),
                       m_PcrPid(0x1fff),
                       m_AType(0),
                       m_VType(0) {
                /*EMPTY*/
            };
            virtual ~PMTEvt() {
                /*EMPTY*/
            };
            int m_APid,m_VPid,m_PcrPid;
            int m_AType,m_VType;
    };

    /*!
    * \brief Signals new raw section
    *
    *  m_Sender points to Pad which sends this event. FYI pad points to dmx(GetFather())
    */
    class SectionEvt: public evt::Event {
        public:
            SectionEvt(unsigned char *a_buf, size_t a_Len, Pad *a_Pad)
                    : m_RawData(NULL),
                      m_Sender(a_Pad),
                      m_Len(a_Len)
                      {
                if (a_buf != NULL && a_Len > 0) {
                    m_RawData = new unsigned char[a_Len];
                    memcpy(m_RawData, a_buf, a_Len);
                }
            };
            virtual ~SectionEvt() {
                delete m_RawData;
            };
            unsigned char *m_RawData;
            Pad *m_Sender;
            size_t m_Len;
    };


    /*!
    * \brief Signals new statistics information from IZelement
    *
    *
    */
    class BufferDscrEvt: public evt::Event {
    public:
        BufferDscrEvt(IZelement *a_Originator, Pad* a_Pad, size_t a_bufOccupancy, size_t a_bufSize )
            : m_Originator(a_Originator),
              m_Pad(a_Pad),
              m_bufferBitsOccupancy(a_bufOccupancy),
              m_bufferBitsSize(a_bufSize)
        {
        };
        virtual ~BufferDscrEvt() { };
        IZelement* m_Originator;
        Pad* m_Pad;
        size_t m_bufferBitsOccupancy;  // number of bits waiting for processing in the buffer
        size_t m_bufferBitsSize;       // maximum capacity of bufer, in bits
    };

    /*!
    * \brief Signals new playback statistics information from IZelement
    *        Data can be obtained from A/V decoders
    *        Decoder send information about how many seconds of playback
    *        is avaiable for playing.
    */
    class PlaybackDscrEvt: public evt::Event {
    public:
        PlaybackDscrEvt(IZelement *a_Originator, Pad* a_Pad, uint64_t a_AvaiableSeconds)
            : m_Originator(a_Originator),
              m_Pad(a_Pad),
              m_AvaiableSeconds(a_AvaiableSeconds)
        {
        };
        virtual ~PlaybackDscrEvt() { };
        IZelement* m_Originator;
        Pad* m_Pad;
        size_t m_AvaiableSeconds;  // number of seconds avaiable for playback
    };

    /*!
    * \brief Signals new segments statistics for adaptive bitrate control
    */
    class ABRBufferDscrEvt: public evt::Event {
    public:
        ABRBufferDscrEvt(IZelement *a_Originator,
                Pad* a_Pad,
                uint64_t a_segDurationInSeconds,
                uint64_t a_segDownloadStart,
                uint64_t a_segDownloadEnd,
                uint64_t a_segBitrate,
                int32_t a_segBitrateLowest,
                int32_t a_segBitrateNextLower,
                int32_t a_segBitrateNextHigher,
                int32_t a_segBitrateHighest
                )
            :
                m_Originator(a_Originator),
                m_Pad(a_Pad),
                m_segDurationInSeconds(a_segDurationInSeconds),
                m_segDownloadStart(a_segDownloadStart),
                m_segDownloadEnd(a_segDownloadEnd),
                m_segBitrate(a_segBitrate),
                m_segBitrateLowest(a_segBitrateLowest),
                m_segBitrateNextLower(a_segBitrateNextLower),
                m_segBitrateNextHigher(a_segBitrateNextHigher),
                m_segBitrateHighest(a_segBitrateHighest)
                 { }

        virtual ~ABRBufferDscrEvt() { };
        IZelement* m_Originator;
        Pad* m_Pad;
        uint64_t m_segDurationInSeconds;
        uint64_t m_segDownloadStart;
        uint64_t m_segDownloadEnd;
        uint64_t m_segBitrate;

        int32_t m_segBitrateLowest;
        int32_t m_segBitrateNextLower;
        int32_t m_segBitrateNextHigher;
        int32_t m_segBitrateHighest;

    };


    /*!
    * \brief Changes zelement state to:
    *
    *    ZERO - no resource alocated, no data flow allowed
    *    READY - resource allocated, zelement ready to handle data flow
    *    PLAYING - resource allocated, data flowing through
    *    PAUSE - temporar freez of data flow.
    *
    */
    RCode SetState (State a_State) {
        Start();            //make sure Body() runs
        Kick(new StateEvt(a_State));//simply send "new state" event to Body()
        if (a_State == ZERO)
            Wait(this);
        return mfw::zel::OK;
    }


    State GetState ( ) {//TODO: use Lock()
        return m_State;
    }

    /*!
    * \brief Returns state name as a C-string
    *
    *    Returns a nul terminated C-string of with
    *    the name of the state in "a_State"
    * \a_State - state
    */
    static const char* StateName(State a_State) {
        return (a_State==ZERO ? "ZERO" :
                (a_State==READY ? "READY" :
                 (a_State==PLAYING ? "PLAYING" :
                  (a_State==PAUSED ? "PAUSED" : "????"))));
    }

    virtual uint64_t GetPos ( ) {//TODO: use Lock()
        return m_Position;
    }

    virtual uint64_t GetSize ( ) {//TODO: use Lock()
        return m_Size;
    }

    //TODO: either use Lock()(asynchronous) or send event to Body() with new position(synchronous)
    virtual RCode SetPos (uint64_t a_pos ) {
        m_Position = a_pos;
        return mfw::zel::OK;
    }
    
    virtual uint64_t GetPosPTS() { return (m_ByteRate > 0) ? (uint64_t)m_Position * 90000LL / m_ByteRate : 0;};
    virtual RCode SetPosPTS(uint64_t a_Pos) { m_Position = (uint64_t)a_Pos * m_ByteRate / 90000; return mfw::zel::OK;};
    virtual uint64_t GetBasePTS() {return 0;};
    virtual uint64_t GetBase() {return 0;};
    virtual uint64_t GetSizePTS() {return (m_ByteRate > 0) ? (uint64_t)m_Size * 90000LL / m_ByteRate : 0;};

    /*!
     * setup playback speed
     * default = 100 [mfw::ITrickMode::NORMAL_SPEED_FORWARDS]
     */
    virtual void SetSpeed(int16_t a_Speed = 100)
    {
        m_Speed = a_Speed;
    }
    virtual int16_t GetSpeed()
    {
        return m_Speed;
    }

    std::string GetName() {
        return m_Name;
    }

    /*!
    * \brief Used to send an event to Body() or "kick" it (Kick(NULL))
    *
    */
    void Kick(evt::Event *a_Evt) {
        Lock();
        m_Cmds.push(a_Evt);
        sem_post(&m_Signal);
        UnLock();
    }
    virtual void SetClock(ZClock * a_Clk) {
        m_Clock = a_Clk;
    }
    // some targets (eg. intel) need a common clock base (start of play/pause etc).
    virtual void SetClockBase(uint64_t a_Time) {
        m_ClockBase = a_Time;
    }
    virtual uint64_t GetClockBase() {
        return m_ClockBase;
    }
    Pad* GetOutPad(size_t i) {
        if (i >= m_OutputsPads.size())
            return NULL;
        return m_OutputsPads.at(i);
    }
    Pad* GetInPad(size_t i) {
        if (i >= m_InputsPads.size())
            return NULL;
        return m_InputsPads.at(i);
    }
protected:
    std::string m_Name;//zelements name
    State m_State;     // zelement state
    uint64_t m_Size;     // could be stream length|file size|queue length,...
    uint64_t m_Position; // byte position in file/stream,...
    uint32_t m_ByteRate; // media rate bytes/sec
    std::vector<Pad*> m_InputsPads;  //all input pads are kept here
    std::vector<Pad*> m_OutputsPads; //all output pads are kept here
    pthread_mutex_t m_Lock;          //protects the zelement,should be zos mutex
    std::queue<evt::Event*> m_Cmds;  //command/event queue for this zelement
    sem_t m_Signal;    //Body() sleeps on it
    ZClock *m_Clock;   //reference to zelement providing clock
    uint64_t m_ClockBase;
    int16_t m_Speed;   // playback speed normal = 100 (for source zelements)

    /*!
    * \brief Called withing Body() when zelement receives an event which is not state event
    *
    */
    virtual void OnCmd(evt::Event *a_Evt) =0;

    /*!
    * \brief Called when zelement is about to change state
    *
    *    If zelements can change state it must assign a_State to m_State.
    *    Otherwise state change is interpreted by media app as refused.
    * \a_State - new state
    */
    virtual void OnState(State a_State) =0;

    /*!
    * \brief Called evey time zelement receives command/event
    *
    *    Changing to state ZERO forces Body() to quit so OnIdle is not called!
    */
    virtual void OnIdle() =0;

    void Body() {
        for (;;) {
            evt::Event *evt = NULL;

            sem_wait(&m_Signal);//wait for command/event
            Lock();
            if (!m_Cmds.empty()) {
                evt = m_Cmds.front();
                m_Cmds.pop();
            }
            UnLock();
            if (StateEvt *stateevt = dynamic_cast<StateEvt*>(evt)) {//new state event
                State now = m_State;
                OnState(stateevt->m_State);
                if (stateevt->m_State == ZERO) {//In ZERO we don't have any resources so quit
                    delete evt;
                    Lock();
                    while (!m_Cmds.empty()) {
                        delete m_Cmds.front();
                        m_Cmds.pop();
                    }
                    UnLock();
                    break;
                }
                if (now != m_State){//the state has changed
                    PostEvent(new (std::nothrow) StateChangedEvt(this, m_State));
                }
            }else {
                OnCmd(evt);//received something that is not state change but it's an event
            }
            delete evt;
            evt = NULL;

            OnIdle();//do stuff

            pthread_yield();
        }
    }

    void Lock() {
        pthread_mutex_lock(&m_Lock);
    }
    bool TryLock() {
        return (pthread_mutex_trylock(&m_Lock) == 0);  // true if locked OK
    }
    void UnLock() {
        pthread_mutex_unlock(&m_Lock);
        pthread_yield();  // improve response time
    }
    class SmartLock {
        public:
            SmartLock(IZelement* a_Zel) : m_Zel(a_Zel){
                m_Zel->Lock();
            }
            ~SmartLock() {
                m_Zel->UnLock();
            }
            IZelement* m_Zel;
    };

};

};};
#endif // IZELEMENT_HH
