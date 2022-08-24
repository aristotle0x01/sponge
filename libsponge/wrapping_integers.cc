#include "wrapping_integers.hh"

#include <iostream>
#include <vector>

// Dummy implementation of a 32-bit wrapping integer

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
    uint64_t t64 = 0;
    std::vector<uint64_t> v_vec(3);
    std::vector<uint64_t> d_vec(3);

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
    int c = 0;
    while (c < 3) {
        v_vec[c] = gap + STEP * i;
        d_vec[c] = labs(checkpoint - v_vec[c]);
        i++;
        c++;
    }
    while (true) {
        // monotonically increasing
        if (d_vec[2] >= d_vec[1] and d_vec[1] >= d_vec[0]) {
            t64 = v_vec[0];
            break;
        }
        // decrement then increment
        if (d_vec[2] >= d_vec[1] and d_vec[1] <= d_vec[0]) {
            t64 = v_vec[1];
            break;
        }
        v_vec[0] = v_vec[1];
        d_vec[0] = d_vec[1];
        v_vec[1] = v_vec[2];
        d_vec[1] = d_vec[2];
        v_vec[2] = gap + STEP * i;
        d_vec[2] = labs(checkpoint - v_vec[2]);
        i++;
    }

    return {t64};
}