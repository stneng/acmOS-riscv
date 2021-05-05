//
// Created by Wenxin Zheng on 2021/4/21.
//

#ifndef ACMOS_SPR21_ANSWER_LOCKS_H
#define ACMOS_SPR21_ANSWER_LOCKS_H


int lock_init(struct lock *lock) {
    lock->cpuid = 23333;
    __sync_lock_release(&(lock->locked));
    if (nlock >= MAXLOCKS) BUG("Max lock count reached.");
    locks[nlock++] = lock;
    return 0;
}

void acquire(struct lock *lock) {
    while (__sync_lock_test_and_set(&(lock->locked), 1));
    lock->cpuid = cpuid();
}

// Try to acquire the lock once
// Return 0 if succeed, -1 if failed.
int try_acquire(struct lock *lock) {
    if (__sync_lock_test_and_set(&(lock->locked), 1) == 0) {
        lock->cpuid = cpuid();
        return 0;
    }
    return -1;
}

void release(struct lock *lock) {
    if (!holding_lock(lock)) BUG("Invalid unlock.");
    lock->cpuid = 23333;
    __sync_lock_release(&(lock->locked));
}

int is_locked(struct lock *lock) {
    return lock->locked;
}

// private for spin lock
int holding_lock(struct lock *lock) {
    if (lock->locked && lock->cpuid == cpuid()) return 1;
    else return 0;
}

#endif  // ACMOS_SPR21_ANSWER_LOCKS_H
