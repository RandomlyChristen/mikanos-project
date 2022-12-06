#pragma once

struct Message {
    enum Type {
        kNull,
        kInterruptXHCI,
        kTimerTimeout,
    } type;

    union {
        struct {
            unsigned long timeout;
            int value;
        } timer;
        // ...
    } arg;
};