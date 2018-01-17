#ifndef GENERICWRAPPERINPUT_HH
#define GENERICWRAPPERINPUT_HH

#include "../IZelement.hh"

#include <string>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <functional>

namespace mfw { namespace zel {

/**
  * class GenericWrapper - experimental
  *
  *  Defines state transition functions via functors
  *
  */

class GenericWrapper : virtual public IZelement {
public:
    typedef boost::function<void ()> StateFunctor;
    typedef boost::function<bool (uint8_t*, uint32_t&)> ProducerType;
    typedef boost::function<void (uint8_t*, uint32_t&)> ConsumerType;
    GenericWrapper ( )  {
        m_Name = "GenericWrapper";
        m_Go2ZERO = boost::bind(&GenericWrapper::DummyState, this);
        m_Go2READY = boost::bind(&GenericWrapper::DummyState, this);
        m_Go2PLAYING = boost::bind(&GenericWrapper::DummyState, this);
        m_ProduceData = boost::bind(&GenericWrapper::DummyProducer, this, _1, _2);
        m_ConsumeData = boost::bind(&GenericWrapper::DummyConsumer, this, _1, _2);
    };

    virtual ~GenericWrapper ( ) {};
    
    void SetFunctorZERO(StateFunctor a_Zero) { m_Go2ZERO = a_Zero;};
    void SetFunctorREADY(StateFunctor a_Ready) { m_Go2READY = a_Ready;};
    void SetFunctorPLAYING(StateFunctor a_Playing) { m_Go2PLAYING = a_Playing;};
    void SetFunctorOUT(ProducerType a_A) { m_ProduceData = a_A;};
    void SetFunctorIN(ConsumerType a_A) { m_ConsumeData = a_A;};
   
    Pad* GetPad(Dir a_Dir) {
        Pad* pad = IZelement::GetPad(a_Dir);
    
        SmartLock mylock(this);
        
        if (!VerifyPadExists(pad)) {
            return NULL;//it was created and immediately removed...funny but we have to handle it
        }
    
        pad->SetWrite(new Pad::Delegate<GenericWrapper>(this, &GenericWrapper::Write, pad));
        return pad;
    }

protected:
    ssize_t Write(char *a_Data, size_t a_DataLen, Pad::SynchEvt *a_MetaData, Pad* a_Pad) {
        (void)a_MetaData;
        (void)a_Pad;
        uint32_t tmp = a_DataLen;
        m_ConsumeData(reinterpret_cast<unsigned char*>(a_Data), tmp);
        return tmp;
    }

    void OnCmd(evt::Event*) {};
    void OnState(mfw::zel::State a_State) {
        SmartLock lock();
        
        switch(a_State) {
            case READY:
                if (m_State == ZERO) {
                    m_Go2READY();
                }
                break;
            case PLAYING:
                if (m_State == READY) {
                    m_Go2PLAYING();
                }
                break;
            case PAUSED:
                LOG_WARNING(("Not implemented"));
                break;
            default:
                m_Go2ZERO();
                break;
        }
        m_State = a_State;
    };

    void OnIdle() {
        Lock();
        
        if (m_State == PLAYING) {
            uint8_t buf[4096];
            uint32_t len = sizeof(buf);

            if (m_OutputsPads.size() > 0 && m_ProduceData(buf, len) && len >0) {
                for (Pad::PadItr i=m_OutputsPads.begin(); i != m_OutputsPads.end(); ++i) {
                    while (0 == (*i)->Write(reinterpret_cast<char*>(&buf[0]), len, 0)) {
                        LOG_MESSAGE(("Pipeline is full, waiting"));
                        os::Task::Delay(33);
                    }
                }
            }else {
                LOG_TRACE(("no data..."));
                os::Task::Delay(33);
            }
            UnLock();
            Kick(NULL);
            return;
        }

        UnLock();
    };
private:
    void DummyState() {
        LOG_TRACE(("dummy change state"));
    }
    bool DummyProducer(uint8_t* a_A, uint32_t& a_B) {
        LOG_ERROR(("dummy producer :%p %u", a_A, a_B));
        return false;
    }
    void DummyConsumer(uint8_t* a_A, uint32_t& a_B) {
        LOG_ERROR(("dummy consumer :%p %u", a_A, a_B));
    }
    StateFunctor m_Go2READY, m_Go2PLAYING, m_Go2ZERO;
    ProducerType m_ProduceData;
    ConsumerType m_ConsumeData;
};

inline IZelement* CreateGenericWrapper() {
    return new GenericWrapper();
}

};};

#endif //#ifndef GENERICWRAPPERINPUT_HH
