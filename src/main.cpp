#include "bortoli_thread.h"
#include <cstdint>
#include <cstdio>

System* sys;

ThreadId thread_a;
ThreadId thread_b;

LockId lock_a;
LockId lock_b;

void my_thread_a()
{
    ThreadId myself = sys->current_thread_id();

    LockObtainResult lock_result;
    LockReleaseResult unlock_result;
    while (true)
    {
        printf("thread(%d): olá, sou my_thread_a, estou viva!\n", myself);

        lock_result = sys->lock(lock_a);
        printf("thread(%d): lock a => %s\n", myself, str_LockObtainResult(lock_result).c_str());
        lock_result = sys->lock(lock_b);
        printf("thread(%d): lock b => %s\n", myself, str_LockObtainResult(lock_result).c_str());

        unlock_result = sys->unlock(lock_b);
        printf("thread(%d): unlock b => %s\n", myself, str_LockReleaseResult(unlock_result).c_str());
        unlock_result = sys->unlock(lock_a);
        printf("thread(%d): unlock a => %s %d\n", myself, str_LockReleaseResult(unlock_result).c_str(), unlock_result);

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}

void my_thread_b()
{
    ThreadId myself = sys->current_thread_id();

    LockObtainResult lock_result;
    LockReleaseResult unlock_result;
    while (true)
    {

        printf("thread(%d): olá, sou my_thread_b, estou viva!\n", myself);

        lock_result = sys->lock(lock_b);
        printf("thread(%d): lock b => %s\n", myself, str_LockObtainResult(lock_result).c_str());
        lock_result = sys->lock(lock_a);
        printf("thread(%d): lock a => %s\n", myself, str_LockObtainResult(lock_result).c_str());

        unlock_result = sys->unlock(lock_a);
        printf("thread(%d): unlock a => %s %d\n", myself, str_LockReleaseResult(unlock_result).c_str(), unlock_result);
        unlock_result = sys->unlock(lock_b);
        printf("thread(%d): unlock b => %s\n", myself, str_LockReleaseResult(unlock_result).c_str());

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}

int main(int argc, char** argv)
{
    sys = new System();

    printf("Thread principal tem ID: %d\n", sys->current_thread_id());

    lock_a = sys->make_lock();
    lock_b = sys->make_lock();

    thread_a = sys->make_thread(&my_thread_a);
    thread_b = sys->make_thread(&my_thread_b);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    sys->disown_thread(thread_a);
    sys->disown_thread(thread_b);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    printf("[main] Matei todas minhas threads (não é deadlock!!!)\n");

    // delete sys;

    std::this_thread::sleep_for(std::chrono::milliseconds(60000));

    return 0;
}