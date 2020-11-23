// #include "socket.hh"
#include "tcp_sponge_socket.hh"
#include "util.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

void get_URL(const string &host, const string &path) {
    Address addr = Address(host, "http");
    // TCPSocket socket = TCPSocket();
    CS144TCPSocket socket = CS144TCPSocket();

    // After this line of code is executed,
    // the 3-way handshake is performed
    // and a TCP connectin is established
    // between the client and server.
    socket.connect(addr); 

    // Q: Why are there two consecutive "\r\n"?
    // A: The end of the header section is indicated by 
    //    an empty field line,
    //    resulting in the transmission of two consecutive CR-LF pairs
    // Ref link: https://en.wikipedia.org/wiki/List_of_HTTP_header_fields
    socket.write("GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n");
    
    // Client side won't continue to write messages
    socket.shutdown(SHUT_WR);

    // Keep reading messages from Server side until meeting EOF
    while (!socket.eof()) {
        string res = socket.read();
        cout << res;
    }

    socket.close();


    // Lab4 appended
    socket.wait_until_closed();
    return;
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
