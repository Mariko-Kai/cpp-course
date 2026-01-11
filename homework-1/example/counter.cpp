#include "counter.hpp"

class CounterImpl {
public:
    explicit CounterImpl(int value) : value_(value) {}

    void increment() { ++value_; }
    void decrement() { --value_; }
    int get() const { return value_; }

private:
    int value_;
};

struct Counter {
    CounterImpl* impl;
};

extern "C" {

Counter* counter_create(int initial_value) {
    Counter* counter = new Counter;
    counter->impl = new CounterImpl(initial_value);
    return counter;
}

void counter_destroy(Counter* counter) {
    if (!counter) return;
    delete counter->impl;
    delete counter;
}

void counter_increment(Counter* counter) {
    if (counter) counter->impl->increment();
}

void counter_decrement(Counter* counter) {
    if (counter) counter->impl->decrement();
}

int counter_get(const Counter* counter) {
    return counter ? counter->impl->get() : 0;
}

} // extern "C"
