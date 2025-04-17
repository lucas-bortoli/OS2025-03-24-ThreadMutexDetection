#ifndef _H_BTHREAD
#define _H_BTHREAD

#include <cstdbool>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

typedef int ThreadId;
typedef int LockId;

typedef enum class LockObtainResult
{
    OK,
    LockNotFound,
    YouAlreadyOwnLock,
    DeadlockDetected
} LockObtainResult;

std::string str_LockObtainResult(LockObtainResult r);

typedef enum class LockReleaseResult
{
    OK,
    LockNotFound,
    NotYourLock
} LockReleaseResult;

std::string str_LockReleaseResult(LockReleaseResult r);

typedef void (*ThreadFunction)();

class System
{
  private:
    std::map<ThreadId, std::thread*> threads;

    // quais locks a thread T possui?
    std::map<ThreadId, std::set<LockId>> ownership;

    // quais locks a thread T está aguardando?
    std::map<ThreadId, std::set<LockId>> waiting;

    // mapeia o nosso LockId arbitrário para o mutex de "backend"
    std::map<LockId, std::mutex*> locks;

    std::mutex manager_lock; // para proteger o acesso a matriz e aos locks

    // helper para retornar a thread que obteve determinada lock
    std::optional<ThreadId> find_owner_thread(LockId lock_id);
    bool would_cause_deadlock(ThreadId requester);

  public:
    System();
    ~System();

    LockId make_lock();

    /**
     * Obtém uma lock. Bloqueia a thread caller até a lock ser obtida.
     *
     * - Se essa operação for causar um deadlock, `false` é retornado, e a lock não é obtida.
     * - Se a thread caller já obteve essa lock, um resultado positivo é retornado.
     */
    LockObtainResult lock(LockId lock_id);

    /**
     * Libera a lock obtida pela thread caller.
     */
    LockReleaseResult unlock(LockId lock_id);

    ThreadId current_thread_id();

    ThreadId make_thread(ThreadFunction fn);

    void disown_thread(ThreadId target);
};

#endif