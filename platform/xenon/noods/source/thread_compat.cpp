// SPDX-License-Identifier: GPL-3.0-or-later
#include <thread>

extern "C" {
#include <xenon_soc/xenon_power.h>
}

namespace xenonds_thread_compat {
namespace {

// Presentation exclusively owns context 5. NooDS is configured for three
// software-3D workers, so reserve only contexts 2/3/4 here; this prevents a
// future setting change or launch fallback from stealing the video context.
const int kWorkerCount = 3;
const int kHardwareThreads[kWorkerCount] = { 2, 3, 4 };
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

void* const workers[kWorkerCount] = {
    reinterpret_cast<void*>(&xenonds_noods_thread_worker0),
    reinterpret_cast<void*>(&xenonds_noods_thread_worker1),
    reinterpret_cast<void*>(&xenonds_noods_thread_worker2)
};

} // namespace

int launch(Task* task) {
    for (int slot = 0; slot < kWorkerCount; ++slot) {
        if (tasks[slot]) continue;

        tasks[slot] = task;
        __sync_synchronize();
        void* stack_top = stacks[slot] + kStackSize - 256;
        if (xenon_run_thread_task(
                kHardwareThreads[slot], stack_top, workers[slot]) == 0) {
            return slot;
        }

        // A context unexpectedly owned by another LibXenon task must not
        // freeze the emulator in a retry loop. Release this slot and try the
        // next dedicated context; if none can launch, the synchronous fallback
        // below preserves renderer correctness.
        __sync_synchronize();
        tasks[slot] = 0;
    }

    // This should only be reached if upstream requests more workers than the
    // three contexts reserved by XenonDS. Preserve correctness synchronously.
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
