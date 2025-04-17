#include "bortoli_thread.h"
#include <atomic>
#include <cstdbool>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <queue>
#include <string>

#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif

// Kills a thread with extreme prejudice. No cleanup. No mercy.
void murder_thread(std::thread& t)
{
    if (!t.joinable())
        return; // Already dead or detached.

#if defined(_WIN32)
    // Windows: TerminateThread (brutal, immediate)
    HANDLE handle = t.native_handle();
    TerminateThread(handle, 0);
    CloseHandle(handle);
#elif defined(__unix__) || defined(__APPLE__)
    // Linux/macOS: pthread_cancel (async kill if enabled)
    pthread_cancel(t.native_handle());
#endif

    t.detach(); // Disassociate from C++ thread object.
}

LockId make_lock_id()
{
    static std::atomic<LockId> next_id(0);
    return ++next_id;
}

ThreadId make_thread_id()
{
    static std::atomic<ThreadId> next_id(0);
    return ++next_id;
}

std::string str_LockObtainResult(LockObtainResult r)
{
    switch (r)
    {
        case LockObtainResult::OK:
            return "LockObtainResult::OK";
        case LockObtainResult::LockNotFound:
            return "LockObtainResult::LockNotFound";
        case LockObtainResult::YouAlreadyOwnLock:
            return "LockObtainResult::YouAlreadyOwnLock";
        case LockObtainResult::DeadlockDetected:
            return "LockObtainResult::DeadlockDetected";
        default:
            return "???";
    }
}

std::string str_LockReleaseResult(LockReleaseResult r)
{
    switch (r)
    {
        case LockReleaseResult::OK:
            return "LockReleaseResult::OK";
        case LockReleaseResult::LockNotFound:
            return "LockReleaseResult::LockNotFound";
        case LockReleaseResult::NotYourLock:
            return "LockReleaseResult::NotYourLock";
        default:
            return "???";
    }
}

thread_local ThreadId env_current_thread_id = 0;

std::optional<ThreadId> System::find_owner_thread(LockId lock_id)
{
    for (const auto& [tid, locks] : this->ownership)
    {
        if (locks.count(lock_id))
        {
            return tid;
        }
    }
    return std::nullopt; // thread id não encontrada
}

/**
 * Checa se ao adquirir a lock passada, haverá deadlock.
 *
 * CHAMAR ESSA FUNÇÃO ENQUANTO EM POSSE DO manager_lock!!
 */
bool System::would_cause_deadlock(ThreadId caller_id)
{
    std::set<ThreadId> already_visited;
    std::queue<ThreadId> visit_queue;

    visit_queue.push(caller_id);

    // Algoritmo de detecção de ciclos no grafo das threads. Começamos a busca na thread caller, verificamos quais locks
    // essa thread está aguardando (fila de espera), encontramos os donos dessas locks, e visitamos em seguida os donos
    // das locks. Ao longo das etapas, memorizamos quais threads já visitamos. Se pararmos em uma das threads que já
    // visitamos, há um ciclo, e portanto haverá deadlock.
    while (!visit_queue.empty())
    {
        ThreadId current = visit_queue.front();
        visit_queue.pop();

        if (already_visited.count(current))
        {
            // ciclo encontrado
            // printf("ciclo encontrado\n");
            return true;
        }

        already_visited.insert(current);

        // a thread está aguardando um conjunto de locks. Para cada lock sendo aguardado...
        auto wait_it = this->waiting.find(current);
        if (wait_it != this->waiting.end())
        {
            for (LockId wanted : wait_it->second)
            {
                // pegamos a thread owner dessa lock
                auto owner_thread = this->find_owner_thread(wanted);
                if (owner_thread.has_value())
                {
                    // visitaremos essa thread no futuro
                    visit_queue.push(owner_thread.value());
                }
            }
        }
    }

    // printf("would_cause_deadlock: nenhum ciclo encontrado\n");

    // não há mais threads a visitar; nenhum ciclo encontrado
    return false;
}

System::System()
{
    printf("Alocando lock manager...\n");
}

System::~System()
{
    printf("Dealocando lock manager...\n");
}

LockId System::make_lock()
{
    printf("thread(%d): construindo lock...\n", env_current_thread_id);

    std::unique_lock<std::mutex> guard(this->manager_lock);

    LockId id = make_lock_id();
    this->locks.insert({id, new std::mutex()});

    return id;
}

LockObtainResult System::lock(LockId lock_id)
{
    printf("thread(%d): tentando obter lock %d...\n", env_current_thread_id, lock_id);

    std::unique_lock<std::mutex> guard(this->manager_lock);

    if (!this->locks.count(lock_id))
    {
        return LockObtainResult::LockNotFound;
    }

    // verificar se o caller já possui essa lock (aí é no-op)
    if (this->ownership[env_current_thread_id].count(lock_id))
    {
        // essa thread já possui a lock
        printf("thread(%d): essa thread já possui a lock.", env_current_thread_id);
        return LockObtainResult::OK;
    }

    // essa lock não tem dono nenhum?
    if (!this->find_owner_thread(lock_id).has_value())
    {
        // lockar
        guard.unlock();
        this->locks[lock_id]->lock(); // blocking

        // atualizar ownership
        guard.lock();
        this->ownership[env_current_thread_id].insert(lock_id);
        return LockObtainResult::OK;
    }

    // nesse caso, a lock está obtida por outra thread, vamos verificar se aguardar a lock aqui causaria um deadlock. se
    // não for causar deadlock, vamos aguardar.
    // a função de checar deadlock depende de estarmos na lista de espera
    this->waiting[env_current_thread_id].insert(lock_id);
    bool gonna_deadlock = would_cause_deadlock(env_current_thread_id);
    this->waiting[env_current_thread_id].erase(lock_id);

    if (gonna_deadlock)
    {
        printf("thread(%d): deadlock detectado! rejeitando locking e causando suicídio da thread...\n",
               env_current_thread_id);
        guard.unlock();
        this->disown_thread(env_current_thread_id); // suicidio
        return LockObtainResult::DeadlockDetected;
    }

    // [é seguro aguardar pela lock]
    this->waiting[env_current_thread_id].insert(lock_id);

    // enquanto aguardamos, vamos liberar a lock do manager
    guard.unlock();
    this->locks[lock_id]->lock(); // blocking

    // obtivemos a lock! sair da lista de espera
    guard.lock();
    this->ownership[env_current_thread_id].insert(lock_id);
    this->waiting[env_current_thread_id].erase(lock_id);

    return LockObtainResult::OK;
}

LockReleaseResult System::unlock(LockId lock_id)
{
    printf("thread(%d): tentando devolver lock %d...\n", env_current_thread_id, lock_id);

    std::unique_lock<std::mutex> guard(this->manager_lock);

    if (!this->locks.count(lock_id))
    {
        return LockReleaseResult::LockNotFound;
    }

    // verificar se a thread está tentando desbloquear uma lock que não foi obtida por ela
    auto lock_owner = this->find_owner_thread(lock_id);
    if (lock_owner.has_value() && lock_owner.value() != env_current_thread_id)
    {
        return LockReleaseResult::NotYourLock;
    }

    // vamos desbloquear. primeiro, vamos desapegar dessa lock
    this->ownership[env_current_thread_id].erase(lock_id);
    guard.unlock();
    this->locks[lock_id]->unlock();

    return LockReleaseResult::OK;
}

/* coisas de multithreading */

typedef void (*ThreadFunction)();

struct ZygoteArgs
{
    ThreadId client_thread_id;
    ThreadFunction client_fn;
};

void thread_zygote(ZygoteArgs args)
{
    env_current_thread_id = args.client_thread_id;

    printf("thread_zygote(%d): olá, mundo! chamando função principal...\n", env_current_thread_id);
    args.client_fn();
    printf("thread_zygote(%d): finalizando...\n", env_current_thread_id);
}

ThreadId System::current_thread_id()
{
    return env_current_thread_id;
}

ThreadId System::make_thread(ThreadFunction fn)
{
    std::unique_lock<std::mutex> guard(this->manager_lock);

    ThreadId id = make_thread_id();
    ZygoteArgs zygote_args = ZygoteArgs{
        .client_thread_id = id,
        .client_fn = fn,
    };

    auto thread = new std::thread(thread_zygote, zygote_args);
    this->threads.insert({id, thread});

    return id;
}

void System::disown_thread(ThreadId target_id)
{
    printf("thread(%d): tentando disown thread %d...\n", env_current_thread_id, target_id);

    std::unique_lock<std::mutex> guard(this->manager_lock);

    if (!this->threads.count(target_id))
    {
        // thread não existe
        printf("thread(%d): thread %d não existe\n", env_current_thread_id, target_id);
        return;
    }

    for (LockId owned_lock : this->ownership[target_id])
    {
        // liberar a lock
        this->locks[owned_lock]->unlock();
    }

    this->ownership[target_id].clear();
    this->waiting[target_id].clear();

    auto target_thread = this->threads.at(target_id);
    this->threads.erase(target_id);
    murder_thread(*target_thread);
    delete target_thread;
}
