//
//  net_server.h
//

#ifndef NET_SERVER_H
#define NET_SERVER_H

#include "core/comm_common.h"
#include "core/comm_connection.h"
#include "core/comm_message.h"
#include "core/comm_tsqueue.h"

#include <memory>
#include <thread>

namespace gpw {
namespace net {

using boost::asio::ip::tcp;

template <typename T> class server_interface {
  public:
    server_interface (uint16_t port)
        : _acceptor (_context, tcp::endpoint (tcp::v4(), port))
        , _secure{false}
        , _ssl_context{boost::asio::ssl::context::sslv23} {}

    ~server_interface () { stop(); }

    bool
    start () {
        try {
            // Issue some work.
            wait_client_connection();

            // The order is important here.  There should be some work to do
            // before running the context's run().
            _thread_context = std::thread ([this] () { _context.run(); });
        } catch (std::exception& e) {
            // Something prohibited the server from listening
            gpw::net::error ("[SERVER] Exception: {}", e.what());
            return false;
        }

        _done_thread_monitor_connection = false;
        _thread_monitor_connection      = std::thread{&server_interface::monitor, this};

        gpw::net::info ("[SERVER] Started!");
        return true;
    }

    bool
    start (const fs::path& cert_file, const fs::path& keyFile, const fs::path& dhparam) {

        _ssl_context.set_options (
            boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use
        );
        _ssl_context.set_password_callback (std::bind (&server_interface<T>::_getPassword, this));
        _secure = true;

        try {
            // Certificate file
            gpw::net::info ("Certificate file: {}", cert_file.string());
            _ssl_context.use_certificate_chain_file (cert_file.string());

            // Private key file
            gpw::net::info ("Private key file: {}", keyFile.string());
            _ssl_context.use_private_key_file (keyFile.string(), boost::asio::ssl::context::pem);

            // DH file
            gpw::net::info ("DH file: {}", dhparam.string());
            _ssl_context.use_tmp_dh_file (dhparam.string());
        } catch (std::exception& e) {
            // Something prohibited the server from listening
            gpw::net::error ("[SERVER] Exception: {}", e.what());
            return false;
        }

        return start();
    }

    void
    stop () {
        // Request the context to close.  It might take some time.
        _context.stop();

        // Tidy up the context thread
        if (_thread_context.joinable()) _thread_context.join();

        _done_thread_monitor_connection = true;
        if (_thread_monitor_connection.joinable()) _thread_monitor_connection.join();

        // Inform someone, anybody, if they care...
        gpw::net::info ("[SERVER] Stopped!");
    }

    void
    monitor () {
        while (!_done_thread_monitor_connection) {
            for (auto& c : _connections) {
                if (c && !c->is_connected()) {
                    on_client_disconnect (c);
                    delete_client (c);
                }
            }
            std::this_thread::sleep_for (std::chrono::milliseconds (250));
        }
    }

    // ASYNC - Instruct asio to wait for connection
    void
    wait_client_connection () {
        _acceptor.async_accept ([this] (std::error_code ec, tcp::socket socket) {
            if (!ec) {
                // Print the client's ip address.
                std::stringstream epstr;
                epstr << socket.remote_endpoint();
                gpw::net::info ("[SERVER] New connection: {}", epstr.str());

                std::shared_ptr<connection<T>> newconn{nullptr};

                if (_secure) {  // Secure communication

                    // Temporarily create a new connection object.
                    // Because the client and server use the same connection object, we let
                    // the connection object know how to behave by giving the first argument here.
                    // That is the connection object is owned by a server.
                    // The ASIO context object in this server object is shared by all the
                    // connections. Let the connection object know to which message queue to put in
                    // a new message object.  The message queue is also shared among all the
                    // connections.
                    newconn = std::make_shared<connection<T>> (
                        _context, _ssl_context, std::move (socket), _messages_in
                    );
                } else {  // Non-encrypted communication

                    // Temporarily create a new connection object.
                    // Because the client and server use the same connection object, we let
                    // the connection object know how to behave by giving the first argument here.
                    // That is the connection object is owned by a server.
                    // The ASIO context object in this server object is shared by all the
                    // connections. Let the connection object know to which message queue to put in
                    // a new message object.  The message queue is also shared among all the
                    // connections.
                    newconn = std::make_shared<connection<T>> (
                        connection<T>::owner::server, _context, std::move (socket), _messages_in
                    );
                }

                // Give the user server a chance to deny connection
                if (on_client_connect (newconn)) {
                    // connection allowed, so add to container of new connections
                    _connections.push_back (std::move (newconn));

                    _connections.back()->connect_to_client (this, _id_counter++);

                    gpw::net::info ("[{}] connection Approved.", _connections.back()->get_id());
                } else {
                    gpw::net::info ("[-----] connection Denied");

                    // Note that because we use smart pointer for the new connection object,
                    // it will go out of the scope and automatically deleted when the connection is
                    // denied.
                }
            } else {
                // Error has occurred during acceptance.
                gpw::net::error ("[SERVER] New connection Error: {}", ec.message());
            }

            // Prime the asio context with more work - again simply wait for
            // another connection...
            wait_client_connection();
        });
    }

    // Send a message to a specific client
    void
    message_client (std::shared_ptr<connection<T>> client, const message<T>& msg) {
        if (client && client->is_connected()) {
            // Client is valid and still connected.
            client->send (msg);
        } else {
            // Client is disconnected.
            on_client_disconnect (client);
            delete_client (client);
        }
    }

    // Send message to all clients
    void
    message_all_clients (
        const message<T>&              msg,
        std::shared_ptr<connection<T>> ignore_client = nullptr
    ) {
        bool invalid_client_exists = false;

        for (auto& client : _connections) {
            // Check client is connected...
            if (client && client->is_connected()) {
                // ...it is!
                if (client != ignore_client) client->send (msg);
            } else {
                // The client couldn't be contacted, so assume it has disconnected.
                on_client_disconnect (client);
                client.reset();
                invalid_client_exists = true;
            }
        }

        // Call the expensive erase-remove idiom only once.
        // Also note that if we modify the deque while iterating it, the iterator
        // will be invalid and cause runtime issues.
        if (invalid_client_exists)
            _connections.erase (
                std::remove (_connections.begin(), _connections.end(), nullptr), _connections.end()
            );
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
            on_message (msg.remote, msg.msg);

            count_messages++;
        }
    }

  protected:
    // Called when a client connects, you can veto the connection by returning false
    virtual bool
    on_client_connect (std::shared_ptr<connection<T>> /* client */) {
        return false;
    }

    // Called when a client appears to have disconnected
    virtual void
    on_client_disconnect (std::shared_ptr<connection<T>> /* client */) {}

    // Called when a message arrives
    virtual void
    on_message (std::shared_ptr<connection<T>> /* client */, message<T>& /* msg */) {}

    void
    delete_client (std::shared_ptr<connection<T>>& c) {
        c.reset();

        // Erase-Remove idiom to entirely remove the client from the deque.
        // Note that this operation is expensive.
        _connections.erase (
            std::remove (_connections.begin(), _connections.end(), nullptr), _connections.end()
        );
    }

  public:
    // Called when a client is validated
    virtual void
    on_client_validated (std::shared_ptr<connection<T>> /* client */) {}

  protected:
    std::string
    _getPassword () const {
        return "ThisIsSecurePassword";
    }

    // Thread safe queue for incoming message packets
    tsqueue<owned_message<T>> _messages_in;

    // Container of active validated connections
    std::deque<std::shared_ptr<connection<T>>> _connections;

    // Order of declaration is important - it is also the order of initialization
    boost::asio::io_context _context;  // Shared among all the connections
    std::thread             _thread_context;

    // These things need an asio context
    tcp::acceptor _acceptor;

    // Clients will be identified in the "wider system" via an ID
    uint32_t _id_counter = 10000;

    bool                      _secure;
    boost::asio::ssl::context _ssl_context;

    // connection monitoring
    std::thread _thread_monitor_connection;
    bool        _done_thread_monitor_connection;
};

}  // namespace net
}  // namespace gpw

#endif
