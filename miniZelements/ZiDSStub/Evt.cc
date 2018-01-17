#include <zinc/evt.hh>

int Event::m_EventGeneratorNoOfClients = 0;
pthread_mutex_t Event::m_Lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Event::m_LockRemRec = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
std::queue<EvtBase*> Event::m_EvtQueue;
sem_t Event::m_Signal;
std::list<EventHndlKeeper*> Event::m_RecepientsList;
EventGenerator Event::m_EventGenerator;
