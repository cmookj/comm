//
//  SimpleServer.cpp
//  NetServer
//
//  Created by Changmook Chun on 2020/12/28.
//

#include <iostream>

#include "core/comm.h"

enum class custom_message_types : uint32_t {
    server_accept,
    server_deny,
    server_ping,
    message_all,
    server_message,
};

class custom_server : public gpw::net::server_interface<custom_message_types> {
  public:
    custom_server (uint16_t nPort)
        : gpw::net::server_interface<custom_message_types> (nPort) {}

  protected:
    virtual bool
    on_client_connect (
        std::shared_ptr<gpw::net::connection<custom_message_types>> client
    ) override {
        gpw::net::message<custom_message_types> msg;
        msg.set_id (custom_message_types::server_accept);
        client->send (msg);

        return true;
    }

    // Called when a client appears to have disconnected
    virtual void
    on_client_disconnect (
        std::shared_ptr<gpw::net::connection<custom_message_types>> client
    ) override {
        // std::cout << "Removing client [" << client->get_id() << "]\n";
        gpw::net::info ("Removing client [{}]", client->get_id());
    }

    // Called when a message arrives
    virtual void
    on_message (
        std::shared_ptr<gpw::net::connection<custom_message_types>> client,
        gpw::net::message<custom_message_types>&                    msg
    ) override {
        switch (msg.id()) {
        case custom_message_types::server_ping: {
            // std::cout << "[" << client->get_id() << "]: Server Ping\n";
            gpw::net::info ("[{}]: Server Ping", client->get_id());

            // Simply bounce message back to client
            client->send (msg);
        } break;

        case custom_message_types::message_all: {
            // std::cout << "[" << client->get_id() << "]: message All\n";
            gpw::net::info ("[{}]: message All", client->get_id());
            gpw::net::message<custom_message_types> msg;
            msg.set_id (custom_message_types::server_message);
            msg << client->get_id();
            message_all_clients (msg, client);
        } break;

        default: break;
        }
    }
};

int
main (int argc, const char* argv[]) {
    custom_server server (60000);
    server.start();

    while (1) {
        server.update();
    }

    return 0;
}
