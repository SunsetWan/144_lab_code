#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    // bool result = false;
    size_t absSeqno = 0;
    size_t length;
    if (seg.header().syn) { // This seg's syn bit == 1.
        if (_synFlag) {
            return false; // This segment is unacceptable.
        }

        _synFlag = true;
        // result = true;

        // if and only if this seg's syn bit == 1 and TCPReceiver's _synFlag == false, set _isn
        _isn = seg.header().seqno.raw_value();
        absSeqno = 1;
        _base = 1;

        // length_in_sequence_space(): Segment's length in sequence space.
        // Note: Equal to payload length plus one byte if SYN is set, plus one byte if FIN is set.
        length = seg.length_in_sequence_space() - 1;
        
        if (length == 0) { 
            return true; // This segment only have a SYN flag
        }
        
    } else if (!_synFlag) { // TCPReceiver didn't receive a SYN and this seg's SYN bit == 0
        return false;
    } else { // TCPReceiver has already received a SYN, but this seg's SYN == 0.
        WrappingInt32 isn = WrappingInt32(_isn);

        // In your TCP implementation, 
        // you’ll use the index of the last reassembled byte as the checkpoint.
        absSeqno = unwrap(seg.header().seqno, isn, absSeqno);

        length = seg.length_in_sequence_space();
    }

    if (seg.header().fin) { // This seg's FIN bit == 1
        if (_finFlag) { // TCPReciever has already received FIN
            return false;
        }
        _finFlag = true;
        // result = true;
    } else if (seg.length_in_sequence_space() == 0 && absSeqno == _base) {
        return true;
    } else if (absSeqno >= _base + window_size() || absSeqno + length <= _base) {
        // if (!result) {
        //     return false;
        // }
        return false;
    }

    _reassembler.push_substring(seg.payload().copy(), absSeqno - 1, seg.header().fin);
    _base = _reassembler.headIndex() + 1;
    if (_reassembler.isInputEnded()) {
        _base++;
    }
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_base > 0) {
        return wrap(_base, WrappingInt32(_isn));
    } else {
        return std::nullopt;
    }
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
