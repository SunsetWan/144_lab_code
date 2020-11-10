#include "stream_reassembler.hh"
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! merge elm2 to elm1, return merged bytes
long StreamReassembler::merge_block(block_node & elem1, const block_node & elem2) {
    block_node x, y;
    if (elem1.begin < elem2.begin) {
        x = elem1;
        y = elem2;
    } else {
        x = elem2;
        y = elem1;
    }

    // It's not necessary to merge because there are
    if (x.begin + x.length < y.begin) {
        return -1;
    } else if (x.begin + x.length >= y.begin + y.length) {
        elem1 = x;
        return y.length;
    } else {
        elem1.begin = x.begin;
        size_t offset = x.begin + x.length - y.begin;
        elem1.data = x.data + y.data.substr(offset);
        elem1.length = elem1.data.length();
        // cout << "merged bytes :" << x.begin + x.length - y.begin << endl;
        return x.begin + x.length - y.begin;
    }
}

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}




//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index >= _headIndex + _capacity) { // capacity is full, we have to leave this packet alone.
        return;
    }


    block_node elem;
    if (index + data.length() <= _headIndex) {
        // Two situation:
        // 1. this packet is duplicated, so we ignore it.
        // 2. all data have already been accepted, go to check the eof property of this packet.
        if (eof) {
            _eofFlag = true;
        }
        if (eof && empty()) {
            _output.end_input();
        }
    } else if (index < _headIndex) {
        // Also the same, two situation
        // 1. this packet's index < _headIndex
        // this means part of this packet is duplicated, which needs merging.
        size_t offset = _headIndex - index;
        elem.begin = index + offset;
        elem.data = string().assign(data.begin() + offset, data.end());
        elem.length = elem.data.length();

    } else {
        // 2. this packet's index > _headIndex
        elem.begin = index;
        elem.length = data.length();
        elem.data = data;
    }

    _unassembledByteAmount += elem.length;

    // Start merging;
    long merged_bytes = 0;

    // find one element which is not less than the elem,
    // if can't find, return end() iterator
    auto iter = _blocks.lower_bound(elem); 
    cout << "if iter equals end()?: " << (iter == _blocks.end()) << endl;
    while (iter != _blocks.end() && ((merged_bytes = merge_block(elem, *iter)) >= 0)) { // merge next
        _unassembledByteAmount -= merged_bytes;
        _blocks.erase(iter);
        iter = _blocks.lower_bound(elem);
    }

    // There is a set, and if its begin() == end(), what info we can get is this set is empty.
    if (iter != _blocks.begin()) { // merge previous
        iter--;
        while ((merged_bytes = merge_block(elem, *iter)) >= 0) {
            _unassembledByteAmount -= merged_bytes;
            _blocks.erase(iter);
            iter = _blocks.lower_bound(elem);
            cout << "if iter equals end()?: " << (iter == _blocks.end()) << endl;
            if (iter == _blocks.begin()) { // implicit suggestion: _block is empty!
                break; 
            }
            iter--;
        }
    }
    _blocks.insert(elem);
    
    // Write to ByteStream
    if (!_blocks.empty() && _blocks.begin()->begin == _headIndex) {
        const block_node headBlock = *_blocks.begin();
        size_t writtenBytesAmount = _output.write(headBlock.data);
        _headIndex += writtenBytesAmount; // update _headIndex
        _unassembledByteAmount -= writtenBytesAmount; // update _unassembledByteAmount
        _blocks.erase(_blocks.begin());
    }

    checkEof(eof);
}

void StreamReassembler::checkEof(const bool eof) {
    if (eof) {
        _eofFlag = true;
    }
    if (_eofFlag && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    return _unassembledByteAmount;
}

bool StreamReassembler::empty() const {
    return _unassembledByteAmount == 0;
}
