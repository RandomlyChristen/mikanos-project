#pragma once

struct Message {
    enum Type {
        kNull,
        kInterruptXHCI,
    } type;
};