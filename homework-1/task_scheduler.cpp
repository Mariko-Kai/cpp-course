#include "task_scheduler.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

struct TaskScheduler {
  struct InternalTask {
    task_id_t id;
    std::string name;
    uint64_t period_ms;
    uint64_t next_run_ms;
  };

  task_id_t next_id = 1;

  std::unordered_map<task_id_t, InternalTask> tasks;
  std::queue<task_id_t> ready_tasks;
};

extern "C" {

TaskScheduler *scheduler_create(void) { return new TaskScheduler(); }

void scheduler_destroy(TaskScheduler *scheduler) { delete scheduler; }

task_id_t scheduler_add_task(TaskScheduler *scheduler, const char *name,
                             uint64_t period_ms, uint64_t start_ms) {
  if (!scheduler || !name)
    return 0;

  task_id_t id = scheduler->next_id++;

  TaskScheduler::InternalTask task;
  task.id = id;
  task.name = name;
  task.period_ms = period_ms;
  task.next_run_ms = start_ms;

  scheduler->tasks[id] = task;

  return id;
}

bool scheduler_remove_task(TaskScheduler *scheduler, task_id_t id) {
  if (!scheduler)
    return false;

  return scheduler->tasks.erase(id) > 0;
}

bool scheduler_get_task_info(TaskScheduler *scheduler, task_id_t id,
                             TaskInfo *info) {
  if (!scheduler || !info)
    return false;

  auto iterator = scheduler->tasks.find(id);
  if (iterator == scheduler->tasks.end()) {
    return false;
  }

  const auto &task = iterator->second;
  info->id = task.id;
  // Полагаю, что эта команда кросплатформенная (до этого использовал strncpy)
  std::snprintf(info->name, sizeof(info->name), "%s", task.name.c_str());
  info->period_ms = task.period_ms;
  info->next_run_ms = task.next_run_ms;

  return true;
}

size_t scheduler_get_task_count(TaskScheduler *scheduler) {
  if (!scheduler)
    return 0;
  return scheduler->tasks.size();
}

void scheduler_update(TaskScheduler *scheduler, uint64_t now_ms) {
  if (!scheduler)
    return;

  struct Event {
    uint64_t run_time;
    task_id_t id;
    bool operator<(const Event &other) const {
      if (run_time != other.run_time)
        return run_time < other.run_time;
      return id < other.id;
    }
  };

  std::vector<Event> events;
  std::vector<task_id_t> to_delete;

  for (auto &pair : scheduler->tasks) {
    auto &task = pair.second;
    while (task.next_run_ms <= now_ms) {
      events.push_back({task.next_run_ms, task.id});
      if (task.period_ms > 0) {
        task.next_run_ms += task.period_ms;
      } else {
        to_delete.push_back(task.id);
        break;
      }
    }
  }

  std::sort(events.begin(), events.end());

  for (const auto &event : events) {
    scheduler->ready_tasks.push(event.id);
  }

  for (task_id_t id : to_delete) {
    scheduler->tasks.erase(id);
  }
}

size_t scheduler_get_ready_tasks(TaskScheduler *scheduler,
                                 task_id_t *out_buffer, size_t buffer_size) {
  if (!scheduler || !out_buffer || buffer_size == 0)
    return 0;

  size_t count = 0;
  while (count < buffer_size && !scheduler->ready_tasks.empty()) {
    out_buffer[count++] = scheduler->ready_tasks.front();
    scheduler->ready_tasks.pop();
  }

  return count;
}
}
// extern "C"
