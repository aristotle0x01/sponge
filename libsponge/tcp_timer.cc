#include "tcp_timer.hh"

using namespace std;

TcpTimer::TcpTimer(unsigned int rtx_timeout)
    : _retransmission_timeout{rtx_timeout}, _ms_start_tick{0}, _started{false} {}

void TcpTimer::start(size_t ms_start_tick, unsigned int rtx_timeout) {
    if (_started) {
        return;
    }

    _started = true;
    _ms_start_tick = ms_start_tick;
    _retransmission_timeout = rtx_timeout;
}

void TcpTimer::stop() { _started = false; }

void TcpTimer::restart(size_t ms_start_tick, unsigned int rtx_timeout) {
    stop();
    start(ms_start_tick, rtx_timeout);
}

bool TcpTimer::expire(size_t tick) {
    if (_started and tick >= _ms_start_tick) {
        if ((tick - _ms_start_tick) >= _retransmission_timeout) {
            return true;
        }
    }

    return false;
}
