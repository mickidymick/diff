#ifndef Time_HPP
#define Time_HPP

extern "C" {
    #include <yed/plugin.h>
}

#include <time.h>

clock_t time_start(void) {
    return clock();
}


double time_stop(clock_t start) {
    clock_t end;

    end = clock();
    return ((double) (end - start)) / CLOCKS_PER_SEC;
}

void time_print(double time, char *time_point_name) {
    DBG("%s -- time: %lf (seconds)", time_point_name, time);
}

#endif
