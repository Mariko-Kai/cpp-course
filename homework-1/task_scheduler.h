#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// Непрозрачный тип (используется как указатель)
typedef struct TaskScheduler TaskScheduler;

typedef uint32_t task_id_t;

typedef struct TaskInfo {
  task_id_t id;
  char name[32]; // Фиксированная длина имени задачи
  uint64_t period_ms;
  uint64_t next_run_ms;
} TaskInfo;

// Жизненный цикл
TaskScheduler *scheduler_create(void);
void scheduler_destroy(TaskScheduler *scheduler);

// Управление задачами
task_id_t scheduler_add_task(TaskScheduler *scheduler, const char *name,
                             uint64_t period_ms, uint64_t start_ms);
bool scheduler_remove_task(TaskScheduler *scheduler, task_id_t id);
bool scheduler_get_task_info(TaskScheduler *scheduler, task_id_t id,
                             TaskInfo *info);
size_t scheduler_get_task_count(TaskScheduler *scheduler);

// Работа со временем
void scheduler_update(TaskScheduler *scheduler, uint64_t now_ms);

// Извлечение готовых задач
size_t scheduler_get_ready_tasks(TaskScheduler *scheduler,
                                 task_id_t *out_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // TASK_SCHEDULER_H
