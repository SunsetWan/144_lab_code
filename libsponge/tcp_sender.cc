#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmissionTimeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _bytesInFlight;
}

//! \brief create and send segments to fill as much of the window as possible
void TCPSender::fill_window(bool is_able_to_send_syn) {
    // Before starting sending data, 
    // TCPSender should send SYN segment first if _synFlag = false;
    if (!_synFlag) { 
        if (is_able_to_send_syn) {
            TCPSegment synSeg;
            synSeg.header().syn = 1;
            send_segment(synSeg);
            _synFlag = true;
        }
        return;
    }

    // // set initial window size to 1 when _windowSize 
    // size_t wndSize = _windowSize > 0 ? _windowSize : 1;
    size_t remainingWndSize;
    size_t currentUnackedSegments = _next_seqno - _rcvAckno;

    // if TCPSender's window size != 0 and didn't sent FIN segement, it can continue to send data.
    while ((remainingWndSize = _windowSize - currentUnackedSegments) != 0 && !_finFlag) {
        size_t size = min(TCPConfig::MAX_PAYLOAD_SIZE, remainingWndSize);
        TCPSegment seg;
        string payloadData = _stream.read(size); // TCPSender reads data from Application layer.

        // For ref: https://stackoverflow.com/questions/3413470/what-is-stdmove-and-when-should-it-be-used
        // TLDR: it's a new C++ way to avoid copies.
        seg.payload() = Buffer(std::move(payloadData));

        // Add FIN segement if necessary.
        if (seg.length_in_sequence_space() < remainingWndSize && _stream.eof()) {
            seg.header().fin = 1;
            _finFlag = true;
        }

        // If data read from upper layer is empty and this segment's FIN bit == 0,
        // Don't have to send this segement.
        // Note again: 
        // length_in_sequence_space() equals to payload length plus one byte if SYN is set, plus one byte if FIN is set.
        if (seg.length_in_sequence_space() == 0) {
            return;
        }

        send_segment(seg);

        // In a *while* loop,
        // It's careless that forgetting updating the loop condition!
        currentUnackedSegments = _next_seqno - _rcvAckno;
    }

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t absAckno = unwrap(ackno, _isn, _rcvAckno);

    // The ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
    if (absAckno > _next_seqno) {
        return false;
    }

    // Update sender's window size.
    _windowSize = window_size;

    // The ackno is duplicate(already received)
    if (absAckno <= _rcvAckno) {
        return true;
    }

    // In this lab, TCPReceiver use cumulative acknowledgment!
    // Update _rcvAckno
    _rcvAckno = absAckno;

    // Remove any segment that now have been full acked.
    while (!_segments_outstanding.empty()) {
        TCPSegment seg = _segments_outstanding.front();
        size_t absSeqnoOfthisSeg = unwrap(seg.header().seqno, _isn, _next_seqno);
        if (absSeqnoOfthisSeg + seg.length_in_sequence_space() <= _rcvAckno) {
            _bytesInFlight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
        } else {
            break;
        }
    }

    // The TCPSender may need to ﬁll the window again if new space has opened up.
    fill_window();

    _retransmissionTimeout = _initial_retransmission_timeout;
    _consecutiveRetransmission = 0;

    // For ref: check page 273
    // if there are currently any not-yet-acked segments
    // start timer
    if (!_segments_outstanding.empty()) {
        isTimerRunning = true;
        _timer = 0;
    }

    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    // There still are some segments unacked and the timer are time-out
    if (_timer >= _retransmissionTimeout && !_segments_outstanding.empty()) {

        // Retransmit not-yet-acknowledged segment with smallest seqno
        _segments_out.push(_segments_outstanding.front());
        _consecutiveRetransmission += 1;
        
        // For ref: check page 275
        // Doubling the Timeout Interval
        // This modification provides a limited form of congestion control.
        _retransmissionTimeout *= 2;

        // Reset timer
        isTimerRunning = true;
        _timer = 0;
    }

    // If all outstanding segments get acked,
    // stop timer.
    if (_segments_outstanding.empty()) {
        isTimerRunning = false;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutiveRetransmission;
}

void TCPSender::send_empty_segment() {
    // An empty ACK doesn’t need to be remembered or retransmitted.
    TCPSegment emptySeg;
    emptySeg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(emptySeg);
}

void TCPSender::send_segment(TCPSegment & seg) {
    // For ref: check page 273.
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytesInFlight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_outstanding.push(seg);
    if (!isTimerRunning) { // Start timer
        isTimerRunning = true;
        _timer = 0;
    }
}
