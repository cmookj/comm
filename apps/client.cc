//
//  SimpleClient.cpp
//  NetClient
//
//  Created by Changmook Chun on 2020/12/26.
//

#include <chrono>
#include <iostream>
#include <signal.h>
#include <thread>

#include "core/comm.h"

static volatile int keep_running = 1;

void
intHandler (int) {
    keep_running = 0;
}

// -----------------------------------------------------------------------------
//                                 Message IDs for simple ping server and client
// -----------------------------------------------------------------------------
enum class custom_message_types : uint32_t {
    server_accept,
    server_deny,
    server_ping,
    message_all,
    server_message,
};

// -----------------------------------------------------------------------------
//                                                            Simple ping client
// -----------------------------------------------------------------------------
class custom_client : public gpw::net::client_interface<custom_message_types> {
  public:
    void
    ping_server () {
        gpw::net::message<custom_message_types> msg;
        msg.set_id (custom_message_types::server_ping);

        // Caution with this...
        auto time_now = std::chrono::system_clock::now();
        msg << time_now;

        if (is_connected()) send (msg);
    }

    void
    message_all () {
        gpw::net::message<custom_message_types> msg;
        msg.set_id (custom_message_types::message_all);

        if (is_connected()) send (msg);
    }

  protected:
    void
    on_message (gpw::net::message<custom_message_types>& msg) override {
        switch (msg.id()) {
        case custom_message_types::server_accept: {
            // Server has responded to a ping request
            gpw::net::info ("Server accepted connection");
        } break;

        case custom_message_types::server_ping: {
            auto                                  time_now = std::chrono::system_clock::now();
            std::chrono::system_clock::time_point time_then;
            msg >> time_then;
            gpw::net::info (
                "Ping: {}", std::chrono::duration<double> (time_now - time_then).count()
            );
        } break;

        case custom_message_types::server_message: {
            // Server has responded to a ping request
            uint32_t client_id;
            msg >> client_id;
            gpw::net::info ("Hello from [{}]", client_id);
        } break;

        default: gpw::net::info ("Unrecognized message for this client.");
        }
    }
};

// -----------------------------------------------------------------------------
//                                                                          MAIN
// -----------------------------------------------------------------------------
int
main (int argc, const char* argv[]) {
    signal (SIGINT, intHandler);

    // auto c = std::make_unique<custom_client>();
    custom_client c;
    c.connect ("127.0.0.1", 60000);

    while (!c.is_connected()) {
        std::this_thread::sleep_for (std::chrono::seconds (1));
    }
    std::cout << "\n";

    //                                                               Ping thread
    // -------------------------------------------------------------------------
    bool        done_thread_ping{false};
    std::thread thread_ping{[&] () {
        std::size_t counter{0};
        while (!done_thread_ping) {
            if (++counter % 10 == 0) c.message_all();
            else c.ping_server();

            std::this_thread::sleep_for (std::chrono::seconds (1));
        }
    }};

    while (keep_running) {
        c.update();
    }

    std::cout << "Client finished\n";

    done_thread_ping = true;
    if (thread_ping.joinable()) thread_ping.join();
    // if (thread_key_input.joinable()) thread_key_input.join();

    return 0;
}
