#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
 }

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_rcved;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_is_connection_active) {
        return;
    }

    _time_since_last_segment_rcved = 0;

    // We need a ACK segment without payload!
    if (in_syn_sent_state() && seg.header().ack == 1 && seg.payload().size() > 0) {
        return;
    }

    bool send_empty = false;
    if (_sender.next_seqno_absolute() > 0 && seg.header().ack == 1) {
        //
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            send_empty = true;
        }
    }

    // Hand over to TCPReceiver
    bool rcv_flag = _receiver.segment_received(seg);
    if (!rcv_flag) {
        send_empty = true;
    }

    // Try to establish connection
    if (seg.header().syn && in_listen_state()) {
        connect();
        return;
    }

    if (seg.header().rst) {
        // RST segments whose ack flag == 0 should be discarded.
        if (in_syn_sent_state() && !seg.header().ack) {
            return;
        }
        unclean_shutdown(false);
        return;
    }

    if (seg.length_in_sequence_space() > 0) {
        send_empty = true;
    }

    // When do I need to send empty segments?
    // If the segment occupied any sequence numbers, 
    // then you need to make sure it gets acknowledged 
    // at least one segment needs to be sent back to the peer 
    // with an appropriate sequence number and the new ackno and window size.
    if (send_empty) {
        if (_receiver.ackno().has_value() && _sender.segments_out().empty()) { // ?
            //
            _sender.send_empty_segment();
        }
    }

    push_segments_out();
}

bool TCPConnection::active() const {
    return _is_connection_active;
}

size_t TCPConnection::write(const string &data) {
    size_t result = _sender.stream_in().write(data);
    push_segments_out();
    return result;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if (!_is_connection_active) {
        return;
    }
    _time_since_last_segment_rcved += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown(true);
    }
    push_segments_out();
 }

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    push_segments_out();
}

void TCPConnection::connect() {
    push_segments_out(true);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            unclean_shutdown(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::push_segments_out(bool is_active_opener) {
    // If the local is not active opener,
    // the local don't need to send SYN before rcv a SYN
    _sender.fill_window(is_active_opener || in_syn_rcv_state());

    TCPSegment seg;
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = 1;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        if (_need_send_rst) {
            _need_send_rst = false;
            seg.header().rst = 1;
        }
        _segments_out.push(seg);
    }
    clean_shutdown();
    return true;
}

// In an unclean shutdown, 
// the TCPConnection either sends or receives a segment with the rst ﬂag set. 
// In this case, the outbound and inbound ByteStreams should both be in the error state, 
// and active() can return false immediately.
void TCPConnection::unclean_shutdown(bool send_rst) {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _is_connection_active = false;
    if (send_rst) {
        _need_send_rst = true;
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        push_segments_out();
    }
}

// There are four prerequisites to having a clean shutdown:

// Prereq #1 The inbound stream has been fully assembled and has ended.

// Prereq #2 The outbound stream has been ended by the local application 
// and fully sent (including the fact that it ended, i.e. a segment with FIN ) to the remote peer.

// Prereq #3 The outbound stream has been fully acknowledged by the remote peer.

// Prereq #4 The local TCPConnection is conﬁdent that the remote peer can satisfy prerequisite #3.

// Two ways to acheive clean shutdown
// OptionA: lingering after both streams end.
// OptionB: passive close.
bool TCPConnection::clean_shutdown() {
    // Passive Close
    if (_receiver.stream_out().input_ended() && !(_sender.stream_in().eof())) {
        _linger_after_streams_finish = false;
    }

    // _sender.stream_in().eof() = true: fulfill Prereq #2
    // _sender.bytes_in_flight() == 0: fulfill Prereq #3
    // _receiver.stream_out().input_ended(): fulfill Prereq #1
    if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) {
        if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            _is_connection_active = false;
        }
    }

    return !_is_connection_active;
}


bool TCPConnection::in_listen_state() {
    return (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() == 0);
}

// _receiver.stream_out().input_ended() == true: stream reassembler received the string whose eof flag == true
bool TCPConnection::in_syn_rcv_state() {
    return (_receiver.ackno().has_value() && !_receiver.stream_out().input_ended());
}

bool TCPConnection::in_syn_sent_state() {
    return (_sender.next_seqno_absolute() > 0 && _sender.bytes_in_flight() == _sender.next_seqno_absolute());
}


