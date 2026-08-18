// SPDX-License-Identifier: GPL-3.0-or-later
#include <thread>

extern "C" {
#include <xenon_soc/xenon_power.h>
}

namespace xenonds_thread_compat {
namespace {

const int kWorkerCount = 4;
const int kHardwareThreads[kWorkerCount] = { 2, 3, 4, 5 };
const int kStackSize = 64 * 1024;

Task* tasks[kWorkerCount] = {};
alignas(16) unsigned char stacks[kWorkerCount][kStackSize];

void run_slot(int slot) {
    __sync_synchronize();
    Task* task = tasks[slot];
    if (task) task->run();
    __sync_synchronize();
}

extern "C" void xenonds_noods_thread_worker0() { run_slot(0); }
extern "C" void xenonds_noods_thread_worker1() { run_slot(1); }
extern "C" void xenonds_noods_thread_worker2() { run_slot(2); }
extern "C" void xenonds_noods_thread_worker3() { run_slot(3); }

void* const workers[kWorkerCount] = {
    reinterpret_cast<void*>(&xenonds_noods_thread_worker0),
    reinterpret_cast<void*>(&xenonds_noods_thread_worker1),
    reinterpret_cast<void*>(&xenonds_noods_thread_worker2),
    reinterpret_cast<void*>(&xenonds_noods_thread_worker3)
};

} // namespace

int launch(Task* task) {
    for (int slot = 0; slot < kWorkerCount; ++slot) {
        if (tasks[slot]) continue;

        tasks[slot] = task;
        __sync_synchronize();
        void* stack_top = stacks[slot] + kStackSize - 256;
        while (xenon_run_thread_task(
                   kHardwareThreads[slot], stack_top, workers[slot]) != 0) {
        }
        return slot;
    }

    // This should only be reached if upstream requests more workers than the
    // four contexts reserved by XenonDS. Preserve correctness synchronously.
    task->run();
    return -1;
}

void join(int slot) {
    if (slot < 0) return;
    while (xenon_is_thread_task_running(kHardwareThreads[slot]) != 0) {
    }
    __sync_synchronize();
    tasks[slot] = 0;
}

} // namespace xenonds_thread_compat
