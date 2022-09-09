#include "wrapping_integers.hh"

#include <iostream>
#include <vector>

// implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t t64 = (n + isn.raw_value());
    uint64_t t32 = t64 % (1ll << 32);
    return WrappingInt32{static_cast<uint32_t>(t32)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t STEP = (1ll << 32);
    uint64_t gap = 0;
    if (n.raw_value() >= isn.raw_value()) {
        gap = n.raw_value() - isn.raw_value();
    } else {
        gap = STEP - isn.raw_value() + n.raw_value();
    }

    uint64_t multi = checkpoint / STEP;
    uint64_t i = 0;
    if (multi > 2) {
        i = multi - 1;
    }

    uint64_t v1, v2, v3;
    uint64_t d1, d2, d3;
    v1 = gap + STEP * (i++);
    d1 = labs(checkpoint - v1);
    v2 = gap + STEP * (i++);
    d2 = labs(checkpoint - v2);
    v3 = gap + STEP * (i++);
    d3 = labs(checkpoint - v3);

    uint64_t t64 = 0;
    while (true) {
        // monotonically increasing
        if (d3 >= d2 and d2 >= d1) {
            t64 = v1;
            break;
        }
        // decrement then increment
        if (d3 >= d2 and d2 <= d1) {
            t64 = v2;
            break;
        }
        v1 = v2;
        d1 = d2;
        v2 = v3;
        d2 = d3;
        v3 = gap + STEP * i;
        d3 = labs(checkpoint - v3);
        i++;
    }

    return {t64};
}