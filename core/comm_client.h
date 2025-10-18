//
//  net_client.h
//

#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include "core/comm_common.h"
#include "core/comm_connection.h"
#include "core/comm_message.h"
#include "core/comm_tsqueue.h"

namespace gpw {
namespace net {

using boost::asio::ip::tcp;

template <typename T> class client_interface {
  public:
    client_interface ()
        : _secure{false} {
        // Initialize the socket with the io context, so it can do stuff
        start_monitoring();
    }

    ~client_interface () {
        stop_waiting();
        stop_monitoring();

        // If the client is destoyed, always try and disconnect from server.
        disconnect();
    }

  public:
    // Connect to server with hostname/ip-address and port
    bool
    connect (
        const std::string& host,
        const uint16_t     port,
        const bool         reconnect = true,
        const std::string& cert_file = ""
    ) {
        _host                = host;
        _port                = port;
        _cert_file           = cert_file;
        _reconnect           = reconnect;
        _connected_initially = false;
        try {
            // Resolve hostname/ip-address into tangiable physical address.
            tcp::resolver               resolver (_context);
            tcp::resolver::results_type endpoints = resolver.resolve (host, std::to_string (port));

            if (cert_file.length() != 0) {  // Secure communication
                _secure = true;
                gpw::net::info ("Creating secure connection.");
                boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
                ssl_context.load_verify_file (cert_file);  // "server.cert"

                // Create a connection
                _connection = std::make_unique<connection<T>> (_context, ssl_context, _messages_in);
            } else {  // Non-encrypted communication
                _secure = false;
                gpw::net::info ("Creating connection.");
                // Create a connection
                _connection = std::make_unique<connection<T>> (
                    connection<T>::owner::client, _context, tcp::socket{_context}, _messages_in
                );
            }

            // Tell the connection object to connect to server.
            // This will post a job to the context.
            _connection->connect_to_server (endpoints);

            // Because we have a job in the context, start context thread.
            _thread_context = std::thread{[this] () { _context.run(); }};
        } catch (std::exception& e) {
            gpw::net::error ("Client exception: {}", e.what());
            return false;
        }
        return true;
    }

    // Disconnect from server
    void
    disconnect () {
        // If connection exists, and it's connected then...
        if (is_connected()) {
            // ...disconnect from server gracefully.
            _connection->disconnect();
        }

        // Either way, we're also done with the asio context...
        _context.stop();
        // ...and its thread
        if (_thread_context.joinable()) _thread_context.join();

        // Destroy the connection object.
        _connection.release();
    }

    // Check if client is actually connected to a server
    bool
    is_connected () {
        if (_connection) return _connection->is_connected();
        else return false;
    }

    void
    start_monitoring () {
        _done_thread_monitor_connection = false;
        _thread_monitor_connection      = std::thread{[this] () { monitor(); }};
    }

    void
    stop_monitoring () {
        _done_thread_monitor_connection = true;
        if (_thread_monitor_connection.joinable()) _thread_monitor_connection.join();
    }

    void
    monitor () {
        while (!_done_thread_monitor_connection) {
            if (_connected_initially) {
                if (_connection && !_connection->is_connected()) {
                    _connected_initially = false;
                    on_disconnect();
                }
            } else {
                if (_connection && _connection->is_connected()) _connected_initially = true;
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (250));
        }
    }

    // This function is called by the user to explicitly process some of the messages
    // in the queue.
    // The argument specifies the maximum number of messages to process with this call.
    void
    update (std::size_t max_count_messages = std::numeric_limits<size_t>::max(), bool wait = true) {
        // We don't need the server to occupy 100% of a CPU core.
        if (wait) _messages_in.wait();

        std::size_t count_messages = 0;
        while (count_messages < max_count_messages && !_messages_in.empty()) {
            // Grab the front message
            auto msg = _messages_in.pop_front();

            // Pass to message handler
            on_message (msg.msg);

            count_messages++;
        }
    }

    void
    stop_waiting () {
        _messages_in.release_wait();
    }

    // Retrieve queue of messages from server.
    tsqueue<owned_message<T>>&
    incoming () {
        return _messages_in;
    }

    void
    send (const message<T>& msg) {
        _connection->send (msg);
    }

    void
    send (message<T>&& msg) {
        _connection->send (msg);
    }

  protected:
    // Called when a message arrives
    virtual void
    on_message (message<T>& /* msg */) {}
    void
    on_disconnect () {
        if (_reconnect) {
            disconnect();
            _context.reset();
            connect (_host, _port, _reconnect, _cert_file);
        }
    }

  protected:
    // asio context handles the data transfer...
    boost::asio::io_context _context;
    // ... but needs a thread of its own to execute its work commands
    std::thread _thread_context;

    // The client has a single instance of a "connection" object, which handles
    // data transfer.
    std::unique_ptr<connection<T>> _connection;

    std::string _host;
    uint16_t    _port;
    std::string _cert_file;

    bool _secure;

    // Connection monitoring and reconnection
    std::thread _thread_monitor_connection;
    bool        _done_thread_monitor_connection;
    bool        _connected_initially;
    bool        _reconnect;

  private:
    // This is the thread safe queue of incoming messages from server
    tsqueue<owned_message<T>> _messages_in;
};

}  // namespace net
}  // namespace gpw

#endif
