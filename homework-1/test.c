#include "task_scheduler.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_scheduler_lifecycle() {
  TaskScheduler *sched = scheduler_create();
  assert(sched != NULL);
  assert(scheduler_get_task_count(sched) == 0);
  scheduler_destroy(sched);
  printf("test_scheduler_lifecycle passed.\n");
}

void test_add_remove() {
  TaskScheduler *sched = scheduler_create();
  task_id_t id1 = scheduler_add_task(sched, "task1", 100, 50);
  assert(id1 != 0);
  assert(scheduler_get_task_count(sched) == 1);

  TaskInfo info;
  assert(scheduler_get_task_info(sched, id1, &info));
  assert(info.id == id1);
  assert(strcmp(info.name, "task1") == 0);
  assert(info.period_ms == 100);
  assert(info.next_run_ms == 50);

  assert(scheduler_remove_task(sched, id1));
  assert(scheduler_get_task_count(sched) == 0);
  assert(!scheduler_get_task_info(sched, id1, &info));

  scheduler_destroy(sched);
  printf("test_add_remove passed.\n");
}

void test_update_and_ready() {
  TaskScheduler *sched = scheduler_create();

  task_id_t t1 = scheduler_add_task(sched, "t1", 0, 10);

  task_id_t t2 = scheduler_add_task(sched, "t2", 30, 20);

  task_id_t t3 = scheduler_add_task(sched, "t3", 0, 20);

  scheduler_update(sched, 0);
  task_id_t buffer[10];
  assert(scheduler_get_ready_tasks(sched, buffer, 10) == 0);

  scheduler_update(sched, 10);
  assert(scheduler_get_ready_tasks(sched, buffer, 10) == 1);
  assert(buffer[0] == t1);
  assert(scheduler_get_task_count(sched) == 2);

  scheduler_update(sched, 20);
  assert(scheduler_get_ready_tasks(sched, buffer, 10) == 2);
  assert(buffer[0] == t2);
  assert(buffer[1] == t3);
  assert(scheduler_get_task_count(sched) == 1);

  scheduler_update(sched, 50);
  assert(scheduler_get_ready_tasks(sched, buffer, 10) == 1);
  assert(buffer[0] == t2);

  scheduler_update(sched, 80);
  assert(scheduler_get_ready_tasks(sched, buffer, 0) == 0);
  assert(scheduler_get_ready_tasks(sched, buffer, 10) == 1);
  assert(buffer[0] == t2);

  scheduler_destroy(sched);
  printf("test_update_and_ready passed.\n");
}

int main() {
  test_scheduler_lifecycle();
  test_add_remove();
  test_update_and_ready();
  printf("All tests passed successfully!\n");
  return 0;
}
