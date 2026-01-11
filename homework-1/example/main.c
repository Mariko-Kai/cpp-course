#include <stdio.h>
#include "counter.hpp"

int main() {
    Counter* c = counter_create(10);

    counter_increment(c);
    counter_increment(c);
    printf("Value = %d\n", counter_get(c));

    counter_decrement(c);
    printf("Value = %d\n", counter_get(c));

    counter_destroy(c);
    return 0;
}
