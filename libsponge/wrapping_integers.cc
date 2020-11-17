#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // Ref: https://stackoverflow.com/questions/103512/why-use-static-castintx-instead-of-intx

    // n mod (2^32)
    // uint32_t downCastValue = static_cast<uint32_t>(n);

    // Another way of n mod (2^32)
    uint32_t downCastValue = n % (static_cast<uint64_t>(0x00000000FFFFFFFF) + 1) ;

    // Streams in TCP can be arbitrarily long—there’s no limit to the length of a ByteStream
    // that can be sent over TCP. 
    // So wrapping is pretty common.
    // So this indicates the low 32 bits of absolute seqno equals the low 32 bits of seqno.
    return WrappingInt32(downCastValue + isn.raw_value());
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
    uint32_t offset = n.raw_value() - isn.raw_value();
    // if checkpoint < 2^32 - 1, t = offset;
    // The low 32 bits of absolute seqno equals the low 32 bits of seqno.
    uint64_t t = (checkpoint & 0xFFFFFFFF00000000) + offset;
    uint64_t result = t;
    if (abs(int64_t(t + (1ul << 32) - checkpoint)) < abs(int64_t(t - checkpoint))) {
        result = t + (1ul << 32);
    }
    if (t >= (1ul << 32) && abs(int64_t(t - (1ul << 32) - checkpoint)) < abs(int64_t(result - checkpoint))) {
        result = t - (1ul << 32);
    }
    return result;

    // uint64_t result = 0;
    // uint64_t twoToThe32Power = (static_cast<uint64_t>(0x00000000FFFFFFFF) + 1);
    // uint64_t divisor = checkpoint / twoToThe32Power;
    // result = divisor * twoToThe32Power + static_cast<uint64_t>(offset);
    // uint64_t diff1 = result > checkpoint ? result - checkpoint : checkpoint - result;
    // uint64_t diff2 = result + twoToThe32Power - checkpoint;
    // if (diff1 < diff2) {
    //     return result;
    // } else {
    //     return result + twoToThe32Power;
    // }
}
