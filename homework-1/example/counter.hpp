#ifndef COUNTER_HPP
#define COUNTER_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Непрозрачный тип — в C это будет просто указатель
typedef struct Counter Counter;

// C-совместимый интерфейс
Counter* counter_create(int initial_value);
void counter_destroy(Counter* counter);

void counter_increment(Counter* counter);
void counter_decrement(Counter* counter);
int  counter_get(const Counter* counter);

#ifdef __cplusplus
}
#endif

#endif // COUNTER_HPP
