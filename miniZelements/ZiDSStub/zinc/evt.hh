#ifndef ZEVT_H
#define ZEVT_H
#include <zinc/zos.hh>

#include <queue>
#include <list>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>


/*! 
* \brief Each event object must inherit from this class 
*
*/
class Event;
class EvtBase
{
public:
    EvtBase()
        : m_Sender(NULL)
    {
    };
    virtual ~EvtBase()
    {
    };
    Event* m_Sender;
};

/*! 
* \brief Doing delegation with only one type is a bit too much so let's give Event and EventGenerator common base 
*
*/
class EventHndl{
public:
    EventHndl(){};
    virtual ~EventHndl(){};
    virtual void OnEvent(EvtBase *a_Evt) =0;
};

/*! 
* \brief Helper object. Assist in recepient removal
*
*/
class EventHndlKeeper{
public:
    EventHndlKeeper(EventHndl* a_Ptr) : ptr(a_Ptr), valid(true){};
    virtual ~EventHndlKeeper(){};
    EventHndl* ptr;
    bool valid;
};

typedef std::list<EventHndlKeeper*>::iterator EvtHndlItr;

class EventGenerator: public os::Task{
public:
    EventGenerator() : os::Task(os::Task::PRIO_7, 1024*1024, "evt_gen"), m_EventGeneratorNoOfClients(NULL), m_Lock(NULL), m_Signal(NULL), m_EvtQueue(NULL), m_RecepientsList(NULL){
    };
    
    virtual ~EventGenerator(){
        sem_post(m_Signal);
        Wait(this);
        pthread_mutex_lock(m_Lock); //clean everything left
        while (!m_EvtQueue->empty()){
            delete m_EvtQueue->front();
            m_EvtQueue->pop();
        }
        for (EvtHndlItr i = m_RecepientsList->begin(); i != m_RecepientsList->end(); ++i){
            delete (*i);
        }
std::cout << "delete EventGenerator\n";
        pthread_mutex_unlock(m_Lock);
    };
    
    void Start(int *a_EventGeneratorRunning, pthread_mutex_t *a_Lock, pthread_mutex_t *a_LockRemRec, sem_t *a_Signal, std::queue<EvtBase*> *a_EvtQueue, std::list<EventHndlKeeper*> *a_RecepientsList){
        m_EventGeneratorNoOfClients = a_EventGeneratorRunning;
        m_Lock = a_Lock;
        m_LockRemRec = a_LockRemRec;
        m_Signal = a_Signal;
        m_EvtQueue = a_EvtQueue;
        m_RecepientsList = a_RecepientsList;
        Task::Start();
    }
    
private:
    void Body(){
        std::list<EventHndlKeeper*> rec_list;
        EvtBase* evt;

        for(;;){
            sem_wait(m_Signal);
            
            pthread_mutex_lock(m_Lock);
            if(!*m_EventGeneratorNoOfClients){//no more clients exit
                pthread_mutex_unlock(m_Lock);
                return;
            }
            if (m_EvtQueue->empty()){
                pthread_mutex_unlock(m_Lock);
                continue;
            }
            rec_list = *m_RecepientsList;//can;t work on the list while it's getting modifies && can;t deliver with lock
            evt = m_EvtQueue->front();
            m_EvtQueue->pop();
            pthread_mutex_unlock(m_Lock);

            for (EvtHndlItr i = rec_list.begin(); i != rec_list.end(); ++i){
                pthread_mutex_lock(m_LockRemRec);//See DontNotifyMe()
                if ((*i)->valid){
                    (*i)->ptr->OnEvent(evt);
                }
                pthread_mutex_unlock(m_LockRemRec);
            }
            delete evt;
        }
    }

    //yeah but the good thing is we can have more than one EventGenerator
    volatile int *m_EventGeneratorNoOfClients;
    pthread_mutex_t *m_Lock;
    pthread_mutex_t *m_LockRemRec;
    sem_t *m_Signal;
    std::queue<EvtBase*> *m_EvtQueue;
    std::list<EventHndlKeeper*> *m_RecepientsList;
};

/*! 
* \brief All obj whishing to send/receive event must inherit from this class
*
*/
class Event: public EventHndl{
public:
    Event(){
        pthread_mutex_lock(&m_Lock);
        if (!m_EventGeneratorNoOfClients++){//the 1st client starts EventGenerator
            sem_init(&m_Signal, 0, 0);
            m_EventGenerator.Start(&m_EventGeneratorNoOfClients, &m_Lock, &m_LockRemRec, &m_Signal, &m_EvtQueue, &m_RecepientsList);
        }
        pthread_mutex_unlock(&m_Lock);
    };
    
    virtual ~Event(){
        DontNotifyMe(this);
        pthread_mutex_lock(&m_Lock);
        --m_EventGeneratorNoOfClients;
        pthread_mutex_unlock(&m_Lock);
    };
    
    /*! 
    * \brief Start event delivery to a_Me
    *
    */
    void NotifyMe(EventHndl *a_Me){
        pthread_mutex_lock(&m_Lock);
        for (EvtHndlItr i = m_RecepientsList.begin(); i != m_RecepientsList.end(); ++i){
            if ((*i)->ptr == a_Me){
                (*i)->valid = true;//if you find out why this is wrong email me, pls:darek.wysokinski@zenterio.net
                pthread_mutex_unlock(&m_Lock);
                return;
            }
        }
        m_RecepientsList.push_back(new EventHndlKeeper(a_Me));
        pthread_mutex_unlock(&m_Lock);
    }

    /*! 
    * \brief Stop event delivery to a_Me
    *
    */
    void DontNotifyMe(EventHndl *a_Me){
        pthread_mutex_lock(&m_LockRemRec);//take list so it doesn;t change while searching
        pthread_mutex_lock(&m_Lock);//case 1: thr x deleting me while EventGenerator delivers to me -> hold deletion until delivered
                                          //case 2: removig meself in OnEvt() -> that's ok but I need to get here. Hence m_LockRemRec is recursive
        for (EvtHndlItr i = m_RecepientsList.begin(); i != m_RecepientsList.end(); ++i){
            if ((*i)->ptr == a_Me){
                (*i)->valid = false;
            }
        }
        pthread_mutex_unlock(&m_Lock);
        pthread_mutex_unlock(&m_LockRemRec);
    }
    /*! 
    * \brief Send an event to all
    *
    */
    void SendEvt(EvtBase *a_Evt){
        pthread_mutex_lock(&m_Lock);
        a_Evt->m_Sender = this;
        m_EvtQueue.push(a_Evt);
        sem_post(&m_Signal);
        pthread_mutex_unlock(&m_Lock);
    }
private:
    /*! 
    * \brief pure error catcher
    *
    */
    virtual void OnEvent(EvtBase *a_Evt){
        (void)a_Evt;
            std::cout << "use DontNotifyMe(this) in Destructor! This time I will do it for you.\n";
    };

    static pthread_mutex_t m_Lock;//protects: m_EventGeneratorNoOfClients, m_EvtQueue, m_Signal, m_RecepientsList 
    static pthread_mutex_t m_LockRemRec;//protects: EventHndlKeeper objects
    static int m_EventGeneratorNoOfClients;
    static std::queue<EvtBase*> m_EvtQueue;
    static sem_t m_Signal;//EventGenerator thread pusher
    static std::list<EventHndlKeeper*> m_RecepientsList;//stores event recepients in form of EventHndlKeeper
    static EventGenerator m_EventGenerator;
};
    /*! 
    * \brief Signals new pmt to mfw::I* implementation
    *
    *   In the end it will be replaced with existing zids section monitor
    */
    class PMTEvt: public EvtBase {
        public:
            PMTEvt() : m_APid(0x1fff),m_VPid(0x1fff),m_AType(0),m_VType(0){};
            virtual ~PMTEvt() {/*EMPTY*/};
            int m_APid,m_VPid;
            int m_AType,m_VType;
    };


namespace evt {

typedef EvtBase Event;

class EventListener : virtual public ::Event {
    public:
};

class EventGenerator : virtual public ::Event {
    public:
        void PostEvent(evt::Event* a_Evt) {
            SendEvt(a_Evt);
        }
        
        void AddListener(evt::EventListener & a_Lis) {
            a_Lis.NotifyMe(dynamic_cast<EventHndl*>(&a_Lis));
        }

        void RemoveListener(evt::EventListener &) {
//            DontNotifyMe(this);
        
        }
};

};
#endif //ZEVT_H
