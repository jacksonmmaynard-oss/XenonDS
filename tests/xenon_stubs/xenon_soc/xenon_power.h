// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define XENON_SPEED_FULL 1
extern "C" void xenon_thread_startup(void);
extern "C" void xenon_make_it_faster(int speed);
extern "C" int xenon_run_thread_task(int thread, void* stack, void* task);
extern "C" int xenon_is_thread_task_running(int thread);
