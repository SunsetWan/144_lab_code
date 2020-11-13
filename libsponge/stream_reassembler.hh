#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <set>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    struct block_node {
      size_t begin = 0;
      size_t length = 0;
      std::string data = "";

      // This operator is a member func. (C++ Primer, p555)
      bool operator<(const block_node rhs) const {
        return this->begin < rhs.begin;
      }
    };

    std::set<block_node> _blocks = {};
    size_t _headIndex = 0;
    size_t _unassembledByteAmount = 0;
    bool _eofFlag = false;

    //! merge elm2 to elm1, return merged bytes
    long merge_block(block_node &elem1, const block_node &elem2);
    void checkEof(const bool eof);

    

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    // Add two getter methods for Lab2
    size_t headIndex() const { return _headIndex; }
    bool isInputEnded() const { return _output.input_ended(); }

};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH


// Here is an example to help myself understand 'goto label' clause
// #include <iostream>
// #include <string>
// #include <set>

// using namespace std;

// void HAHA() {
// 	cout << "HAHA" << endl;
// }

// void goToEqual(int a) {
// 	if (a < 200) {
// 		HAHA();
// 		return;
// 	}
// 	cout << "222" << endl;
// 	HAHA();
// }

// int main(int argc, char *argv[]) {
// 	int a = 400;
// 	if (a < 200) {
// 		goto HAHA;
// 	}
	
// 	cout << "222" << endl;
	
	
// 	HAHA:
// 		cout << "HAHA" << endl;
		
// 	cout << "===========" << endl;
// 	goToEqual(a);
// }
