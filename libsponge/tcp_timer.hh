#ifndef SPONGE_LIBSPONGE_TCP_TIMER_HH
#define SPONGE_LIBSPONGE_TCP_TIMER_HH

#include <cstddef>

class TcpTimer {
  private:
    unsigned int _retransmission_timeout;
    size_t _ms_start_tick;
    bool _started;

  public:
    TcpTimer(unsigned int rtx_timeout);

    void start(size_t ms_start_tick, unsigned int rtx_timeout);
    void stop();
    void restart(size_t ms_start_tick, unsigned int rtx_timeout);
    bool expire(size_t tick);
};

#endif  // SPONGE_LIBSPONGE_TCP_TIMER_HH
