//
//  net_connection.h
//

#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include "core/comm_common.h"
#include "core/comm_message.h"
#include "core/comm_tsqueue.h"

#include <boost/asio/ssl.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

using boost::asio::ip::tcp;

namespace gpw {
namespace net {

// Forward declaration
template <typename T> class server_interface;

template <typename T> class connection : public std::enable_shared_from_this<connection<T>> {
  public:
    enum class owner { server, client };

    using ssl_socket = boost::asio::ssl::stream<tcp::socket>;

    // connection object for non-encrypted communication
    connection (
        owner                      parent,
        boost::asio::io_context&   context,
        tcp::socket                socket,
        tsqueue<owned_message<T>>& qIn
    )
        : _context{context}
        , _socket{std::move (socket)}
        , _ssl_socket{nullptr}
        , _messages_in{qIn}
        , _secure{false}
        , _timer{context} {

        _owner_type = parent;

        // Construct validation check data
        if (_owner_type == owner::server) {
            // connection is Server -> Client, construct random data for the client
            // to transform and send back for validation
            _handshake_out = uint64_t (std::chrono::system_clock::now().time_since_epoch().count());

            // Pre-calculate the result for checking when the client responds.
            _handshake_check = _scramble (_handshake_out);
        } else {
            // connection is Client -> Server, so we have nothing to define.
            _handshake_in  = 0;
            _handshake_out = 0;
        }
    }

    // connection object for secure TLS communication (Client)
    connection (
        boost::asio::io_context&   context,
        boost::asio::ssl::context& ssl_context,
        tsqueue<owned_message<T>>& qIn
    )
        : _context{context}
        , _socket{context}
        , _ssl_socket{nullptr}
        , _messages_in{qIn}
        , _secure{true}
        , _timer{context} {

        _ssl_socket = std::make_unique<ssl_socket> (context, ssl_context);

        _owner_type = owner::client;
        _ssl_socket->set_verify_mode (boost::asio::ssl::verify_peer);
        _ssl_socket->set_verify_callback (
            std::bind (
                &connection<T>::_verify_certificate,
                this,
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    }

    // connection object for secure TLS communication (Server)
    connection (
        boost::asio::io_context&   context,
        boost::asio::ssl::context& ssl_context,
        tcp::socket                socket,
        tsqueue<owned_message<T>>& qIn
    )
        : _context{context}
        , _socket{context}
        , _ssl_socket{nullptr}
        , _messages_in{qIn}
        , _secure{true}
        , _timer{context} {

        _ssl_socket = std::make_unique<ssl_socket> (std::move (socket), ssl_context);

        _owner_type = owner::server;
    }

    ~connection () { disconnect(); }

    void
    connect_to_client (server_interface<T>* server, uint32_t uid = 0) {
        if (_owner_type == owner::server) {
            _id = uid;

            if (_secure) {  // Secure communication
                _handshake_ssl();
            } else {  // Non-encrypted communication
                if (_socket.is_open()) {
                    // Was: _read_header();  // Register task to read messages to the context.

                    // A client has attempted to connect to the server, but we wish the client
                    // to first validate itself, so first write out the handshake data to be
                    // validated.
                    _write_validation();

                    // Next, issue a task to sit and wait asynchronously for precisely the
                    // validation data sent back from the client.
                    _read_validation (server);
                }
            }
        }
    }

    // This method is only called by client.
    void
    connect_to_server (const boost::asio::ip::tcp::resolver::results_type& endpoints) {
        gpw::net::info ("Connecting to server...");
        // Only clients can connect to servers.
        if (_owner_type == owner::client) {
            if (_secure) {  // Secure communication
                boost::asio::async_connect (
                    _ssl_socket->lowest_layer(),
                    endpoints,
                    [this] (std::error_code ec, boost::asio::ip::tcp::endpoint /* endpoint*/) {
                        if (!ec) {
                            _handshake_ssl();
                        }
                    }
                );
            } else {  // Non-encrypted communication
                      // Request asio attempts to connect to an endpoint.
                boost::asio::async_connect (
                    _socket,
                    endpoints,
                    [this] (std::error_code ec, boost::asio::ip::tcp::endpoint /* endpoint */) {
                        if (ec) {
                            gpw::net::error ("connection error: {}", ec.message());
                        } else {
                            // First thing server will do is send packet to be
                            // validated so wait for that and respond.
                            _read_validation();
                        }
                    }
                );

                // It will wait for 10 seconds for the connection.
                // If the connection fails within the deadline, it will retry connection.
                _timer.expires_from_now (boost::posix_time::seconds (10));
                _timer.async_wait ([this, endpoints] (const boost::system::error_code&) {
                    if (!_connected) {
                        gpw::net::error ("connection timeout!");
                        _socket.close();
                        connect_to_server (endpoints);
                    }
                });
            }
        }
    }

    // This method is called by client and server.
    void
    disconnect () {
        if (is_connected()) {
            if (_secure)
                boost::asio::post (_context, [this] () { _ssl_socket->lowest_layer().cancel(); });
            else {
                boost::system::error_code error;
                _socket.shutdown (boost::asio::ip::tcp::socket::shutdown_both, error);
                boost::asio::post (_context, [this] () { _socket.close(); });
            }
        }
        _connected = false;
    }

    // Is the connection is open and valid?
    bool
    is_connected () const {
        // return _secure ? _ssl_socket->lowest_layer ().is_open () : _socket.is_open ();
        return _connected;
    }

    void
    send (const message<T>& msg) {
        // Debugging:
        // std::cout << "TO: " << _id << " " << msg.description() << std::endl;

        // boost::asio::post - posts a job to the context whenever needed.
        // Send the job as a form of a lambda function.
        boost::asio::post (_context, [this, msg] () {
            // Check whether the message queue is empty or not.
            // If the queue is empty, then there is not currently running _write_header()
            // or _write_body() job in the context.
            // Otherwise, there exists a write...() job in the context.
            bool writing_message = !_messages_out.empty();

            // The _write_header() function always pops out the message object from the
            // front end of the deque.  Hence, push back the new message object at the
            // back end of the deque.
            _messages_out.push_back (msg);

            // Prevent posting a new write...() job in the context.
            // There always should be a single write...() job in the context, to prevent
            // out-of-order message writing.
            if (!writing_message) {
                _write_header();
            }
        });
    }

    uint32_t
    get_id () const {
        return _id;
    }

  protected:
    // ASYNC - Prime context ready to read a message header
    void
    _read_header () {
        if (_secure)
            boost::asio::async_read (
                *_ssl_socket,
                boost::asio::buffer (&_temporary_message_in.header(), sizeof (message_header<T>)),
                [this] (const boost::system::error_code& ec, std::size_t /* length */) {
                    if (!ec) {
                        if (_temporary_message_in.size() > 0) {
                            _temporary_message_in.resize (_temporary_message_in.size());
                            _read_body();
                        } else {  // Header-only message
                            _add_to_incoming_message_queue();
                            _read_header();
                        }
                    } else {
                        gpw::net::info ("[{}] Failed to read message header", _id);

                        if ((ec.value() == boost::asio::error::eof) ||
                            (ec.value() == boost::asio::error::connection_reset)) {
                            gpw::net::info ("* connection closed");
                        }

                        if (ec == boost::asio::ssl::error::stream_truncated) {
                            // -> not a real error: ssl connection disconnected.
                            gpw::net::info ("* SSL disconnection");
                        }
                        disconnect();
                        // _socket.close(); // Closing the socket will be detected by the
                        // server or client when either of them tries to send the next message.
                        // When it is detected, the server or client will tidy up.
                    }
                }
            );
        else
            boost::asio::async_read (
                _socket,
                boost::asio::buffer (&_temporary_message_in.header(), sizeof (message_header<T>)),
                [this] (const boost::system::error_code& ec, std::size_t /* length */) {
                    if (!ec) {
                        if (_temporary_message_in.size() > 0) {
                            _temporary_message_in.resize (_temporary_message_in.size());
                            _read_body();
                        } else {  // Header-only message
                            _add_to_incoming_message_queue();
                            _read_header();
                        }
                    } else {
                        gpw::net::info (
                            "[{}] Failed to read message header.  connection closed.", _id
                        );
                        disconnect();
                        // _socket.close(); // Closing the socket will be detected by the
                        // server or client when either of them tries to send the next message.
                        // When it is detected, the server or client will tidy up.
                    }
                }
            );
    }

    // ASYNC - Prime context ready to read a message body
    void
    _read_body () {
        if (_secure)
            boost::asio::async_read (
                *_ssl_socket,
                boost::asio::buffer (_temporary_message_in.data(), _temporary_message_in.size()),
                [this] (const boost::system::error_code& ec, std::size_t /* length */) {
                    if (!ec) {
                        _add_to_incoming_message_queue();
                        _read_header();
                    } else {
                        // As above!
                        gpw::net::info ("[{}] Failed to read message body.", _id);
                        disconnect();
                        // _socket.close();
                    }
                }
            );
        else
            boost::asio::async_read (
                _socket,
                boost::asio::buffer (_temporary_message_in.data(), _temporary_message_in.size()),
                [this] (const boost::system::error_code& ec, std::size_t /* length */) {
                    if (!ec) {
                        _add_to_incoming_message_queue();
                        _read_header();
                    } else {
                        // As above!
                        gpw::net::info ("[{}] Failed to read message body.", _id);
                        disconnect();
                        // _socket.close();
                    }
                }
            );
    }

    // ASYNC - Prime context to write a message header
    void
    _write_header () {
        if (_secure)
            boost::asio::async_write (
                *_ssl_socket,
                boost::asio::buffer (&_messages_out.front().header(), sizeof (message_header<T>)),
                [this] (std::error_code ec, std::size_t /* length */) {
                    if (!ec) {
                        if (_messages_out.front().size() > 0) {
                            _write_body();
                        } else {  // Header-only message.
                            _messages_out.pop_front();

                            if (!_messages_out.empty()) {
                                _write_header();
                            }
                        }
                    } else {
                        gpw::net::info ("[{}] Failed to write message header.", _id);
                        disconnect();
                        // _socket.close();
                    }
                }
            );
        else
            boost::asio::async_write (
                _socket,
                boost::asio::buffer (&_messages_out.front().header(), sizeof (message_header<T>)),
                [this] (std::error_code ec, std::size_t /* length */) {
                    if (!ec) {
                        if (_messages_out.front().size() > 0) {
                            _write_body();
                        } else {  // Header-only message.
                            _messages_out.pop_front();

                            if (!_messages_out.empty()) {
                                _write_header();
                            }
                        }
                    } else {
                        gpw::net::info ("[{}] Failed to write message header.", _id);
                        disconnect();
                        // _socket.close();
                    }
                }
            );
    }

    // ASYNC - Prime context to write a message body
    void
    _write_body () {
        if (_secure)
            boost::asio::async_write (
                *_ssl_socket,
                boost::asio::buffer (_messages_out.front().data(), _messages_out.front().size()),
                [this] (std::error_code ec, std::size_t /* length */) {
                    if (!ec) {
                        _messages_out.pop_front();

                        if (!_messages_out.empty()) {
                            _write_header();
                        }
                    } else {
                        gpw::net::info ("[{}] Failed to write message body.", _id);
                        disconnect();
                        // _socket.close();
                    }
                }
            );
        else
            boost::asio::async_write (
                _socket,
                boost::asio::buffer (_messages_out.front().data(), _messages_out.front().size()),
                [this] (std::error_code ec, std::size_t /* length */) {
                    if (!ec) {
                        _messages_out.pop_front();

                        if (!_messages_out.empty()) {
                            _write_header();
                        }
                    } else {
                        gpw::net::info ("[{}] Failed to write message body.", _id);
                        disconnect();
                        // _socket.close();
                    }
                }
            );
    }

    void
    _add_to_incoming_message_queue () {
        // Debugging:
        // std::cout << "FROM: " << _id << " " << _temporary_message_in.description() <<
        // std::endl;

        if (_owner_type == owner::server)
            // Use initializer_list to construct a new owned_message object.
            _messages_in.push_back ({this->shared_from_this(), _temporary_message_in});
        else  // It is a client
            _messages_in.push_back ({nullptr, _temporary_message_in});
    }

    // "Encrypt" data
    uint64_t
    _scramble (uint64_t nInput) {
        uint64_t out = nInput ^ 0xDEADBEEFC0DECAFE;
        out          = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;
        return out ^ 0XC0DEFACE12345678;
    }

    // ASYNC - Used by both client and server to write validation packet.
    void
    _write_validation () {
        uint64_t handshake_out_in_big_endian = _handshake_out;
        // System native byte order to network byte order:
        boost::endian::native_to_big_inplace (handshake_out_in_big_endian);

        boost::asio::async_write (
            _socket,
            boost::asio::buffer (&handshake_out_in_big_endian, sizeof (uint64_t)),
            [this] (std::error_code ec, std::size_t /* length */) {
                if (!ec) {
                    // Validation data sent, clients should sit and wait
                    // for a response (or a closure)
                    if (_owner_type == owner::client) {
                        _connected = true;
                        _read_header();
                    }
                } else {
                    disconnect();
                    // _socket.close();
                }
            }
        );
    }

    void
    _read_validation (server_interface<T>* server = nullptr) {
        boost::asio::async_read (
            _socket,
            boost::asio::buffer (&_handshake_in, sizeof (uint64_t)),
            [this, server] (std::error_code ec, std::size_t /* length */) {
                if (!ec) {
                    // Network byte order to system native byte order:
                    boost::endian::big_to_native_inplace (_handshake_in);

                    if (_owner_type == owner::server) {
                        if (_handshake_in == _handshake_check) {
                            // Client has provided valid solution, so allow it to
                            // connect properly.
                            gpw::net::info ("Client validated");
                            server->on_client_validated (this->shared_from_this());

                            _connected = true;

                            // Sit waiting to receive data now.
                            _read_header();
                        } else {
                            // Client gave incorrect data, so disconnect.
                            gpw::net::info (
                                "Expecting {}, but received {}\n"
                                "Client disconnected (Failed validation)",
                                _handshake_check,
                                _handshake_in
                            );
                            disconnect();
                            // _socket.close();
                        }
                    } else {
                        // connection is a client, so solve puzzle.
                        _handshake_out = _scramble (_handshake_in);

                        // Write the result.
                        _write_validation();
                    }
                } else {
                    // Some biggerfailure occured.
                    gpw::net::info ("Client disconnected (Failed to read validation)");
                    disconnect();
                    // _socket.close();
                }
            }
        );
    }

    void
    _handshake_ssl () {
        auto self{this->shared_from_this()};
        _ssl_socket->async_handshake (
            _owner_type == owner::server ? boost::asio::ssl::stream_base::server
                                         : boost::asio::ssl::stream_base::client,
            [this, self] (const boost::system::error_code& ec) {
                if (!ec) {
                    _connected = true;
                    _read_header();
                } else gpw::net::info ("Handshaking for SSL failed: {}", ec.message());
            }
        );
    }

    bool
    _verify_certificate (bool preverified, boost::asio::ssl::verify_context& ctx) {
        // The verify callback can be used to check whether the certificate that is
        // being presented is valid for the peer.  For example, RFC 2818 describes
        // the steps involved in doing this for HTTPS.  Consult the OpenSSL
        // documentation for more details.  Note that the callback is called once
        // for each certificate in the certificate chain, starting from the root
        // certificate authority.

        // Here, we will simply print the certificate's subject name.
        char  subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert (ctx.native_handle());
        X509_NAME_oneline (X509_get_subject_name (cert), subject_name, 256);
        gpw::net::info ("Verifying {}", subject_name);

        return preverified;
    }

  protected:
    // This context is shared with the whole asio instance.
    // Even though the server object has multiple connections, all the connections
    // share a single asio context.
    boost::asio::io_context& _context;

    // Each connection has a unique socket to a remote
    tcp::socket _socket;

    // Secure socket for encrypted communication.
    std::unique_ptr<ssl_socket> _ssl_socket;

    // This queue holds all messages to be sent to the remote side of this connection.
    tsqueue<message<T>> _messages_out;

    // This queue holds all messages that have been received from the remote side
    // of this connection.  Note that it is a reference as the "owner" of this connection
    // is expected to provide a queue.
    tsqueue<owned_message<T>>& _messages_in;
    message<T>                 _temporary_message_in;

    // The "owner" decides how some of the connection behaves
    owner _owner_type = owner::server;

    // connection identifier.
    uint32_t _id = 0;

    // Handshake validation
    uint64_t _handshake_out   = 0;
    uint64_t _handshake_in    = 0;
    uint64_t _handshake_check = 0;

    // Status of the message queue
    bool _secure;

    // Timer for the timeout of connect
    boost::asio::deadline_timer _timer;
    bool                        _connected;
};

}  // namespace net
}  // namespace gpw

#endif
