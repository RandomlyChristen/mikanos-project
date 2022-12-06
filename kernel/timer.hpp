#pragma once

#include <cstdint>
#include <queue>
#include <deque>
#include "message.hpp"

class Timer {
public:
    Timer(unsigned long timeout, int value);
    unsigned long Timeout() const { return timeout_; }
    int Value() const { return value_; }

    inline bool operator< (const Timer& rhs) const {
        return this->Timeout() > rhs.Timeout();
    }

private:
    unsigned long timeout_;
    int value_;
};

class TimerManager {
public:
    TimerManager(std::deque<Message>& msg_queue);
    void Tick();
    unsigned long CurrentTick() const { return tick_; }
    void AddTimer(const Timer& timer);

private:
    volatile unsigned long tick_{0};
    std::priority_queue<Timer> timers_{};
    std::deque<Message>& msg_queue_;
};

extern TimerManager* timer_manager;

void LAPICTimerOnInterrupt();
void InitializeLAPICTimer(std::deque<Message>& msg_queue);