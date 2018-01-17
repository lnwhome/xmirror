#ifndef _ZOS_TASK_HH_
#define _ZOS_TASK_HH_

#include <cerrno>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <time.h>

#define ZOS_DBG(format, args...)

/**
 * Namespace for OS-wrapper
 */
namespace os
{

/**
 * Task class
 */
class Task
{
public:
    /**
     * Enumeration of task priorities
     *
     * PRIO_0 is the lowest and PRIO_15 is the highest
     */
    enum Priority
    {
        PRIO_0 = 0,
        PRIO_1,
        PRIO_2,
        PRIO_3,
        PRIO_4,
        PRIO_5,
        PRIO_6,
        PRIO_7,
        PRIO_8,
        PRIO_9,
        PRIO_10,
        PRIO_11,
        PRIO_12,
        PRIO_13,
        PRIO_14,
        PRIO_15
    };

    /**
     * Constructor
     *
     * Constructs a task object.
     * Task will not be started until Start() is called.
     *
     * @param prio       I Task priority
     * @param stacksize  I Stacksize in kb
     * @param name       I Name of the task
     *
     * @see Start()
     */
    inline Task(Priority prio, uint32_t stacksize, const char * name);

    /**
     * Destructor
     */
    inline virtual ~Task();

    /**
     * Start the task
     */
    inline void Start();

    /**
     * Delay the current task
     *
     * @param delay  I  Delay in milliseconds
     */
    static inline void Delay(uint32_t ms);

    /**
     * Wait for a task to terminate
     *
     * @param task
     */
    static inline void Wait(Task * task);

protected:
    /**
     * Task body
     *
     * Needs to be implemented by the subclass. This is the main body
     * of the task run by the start method.
     */
    virtual void Body() = 0;

    /**
     * Helper function to start the task
     *
     * @param data  I  Pointer to task object
     */
    static inline void * RunTask(void * data);

    pthread_t Thread;       /**< Thread id */
    bool ThreadCreatedFlag; /**< Was the task created/started */
};

inline
Task::Task(Task::Priority prio,
           uint32_t stacksize,
           const char * name) :
    Thread(0),
    ThreadCreatedFlag(false)
{
    (void)prio;
    (void)stacksize;
    (void)name;
}

inline
Task::~Task()
{
    if(ThreadCreatedFlag)
    {
        int res;
        if ((res = pthread_join(Thread, NULL)) != 0) {
            ZOS_DBG("Failed to join thread: %s\n", ::strerror(res));
        }
    }
}

inline void
Task::Start()
{
    if (ThreadCreatedFlag) {
        return;
    }

    int ret = pthread_create(&Thread, NULL, RunTask,
                             static_cast<void*>(this));
    if (ret != 0) {
        ZOS_DBG("Failed to create thread in %s:%d\n", __FILE__, __LINE__);
        ZOS_DBG("Stack size: %u Reason: %d, %s\n",
                StackSize, ret, ::strerror(ret));
        assert(!"Failed to create threadin zos");
    }
    else {
        ThreadCreatedFlag = true;
    }
}

inline void
Task::Delay(uint32_t delay)
{
    uint64_t abs64 = delay;
    abs64 *= 1000000;//ms -> ns
    struct timespec ts = { static_cast<__time_t>(abs64 / 1000000000),
                           static_cast<__time_t>(abs64 % 1000000000) };
    if (nanosleep(&ts, NULL) != 0) {
        ZOS_DBG("Failed to sleep in %s:%d\n", __FILE__, __LINE__);
        ZOS_DBG("Reason: %s\n", ::strerror(errno));
    }
}

inline void
Task::Wait(Task * task)
{
    if(task && task->ThreadCreatedFlag) {
        int res;
        if ((res = pthread_join(task->Thread, NULL)) != 0) {
            ZOS_DBG("Failed to join thread: %s\n", ::strerror(res));
        }
        else {
            task->ThreadCreatedFlag = false;
        }
    }
}

inline void*
Task::RunTask(void * data)
{
    if(data) {
        Task * obj = reinterpret_cast<Task*>(data);

        ZOS_DBG("New thread %s (%u / %u)", obj->Name, pthread_self(), (pid_t)syscall(SYS_gettid));

        obj->Body();
    }
    return NULL;
}

} // namespace

#endif
