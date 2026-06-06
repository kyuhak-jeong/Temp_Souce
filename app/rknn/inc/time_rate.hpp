#ifndef TIME_RATE_HPP_
#define TIME_RATE_HPP_

/*** Include ***/
/* for general */
#include <chrono>
#include <thread>

namespace Time {

using TimePoint = std::chrono::_V2::steady_clock::time_point;

class Rate {
public:
    Rate(int rate=1) : m_rate(rate) {
        m_prev_time = std::chrono::steady_clock::now();
    }

    bool sleep(bool do_sleep=true) {
        TimePoint now_time = std::chrono::steady_clock::now();

        double time = (double)(now_time - m_prev_time).count() / 1000000.0;

        bool is_delay = true;
        int delay = (int)((1000.f / (double)m_rate) - time);
        if (delay < 0) {
            delay = 0;
            is_delay = false;
        }

        if (do_sleep) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        
        m_prev_time = std::chrono::steady_clock::now();

        return is_delay;
    }

    void reset() {
        m_prev_time = std::chrono::steady_clock::now();
    }

    void update_rate(int rate) {
        m_rate = rate;
    }

private:
    TimePoint m_prev_time;
    int m_rate;

};

} // namespace Time

#endif