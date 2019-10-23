#pragma once

#include "message.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

using message_queue = std::deque<message>;

class client
{
    enum class state
    {
        NONE,
        CONNECTED,
        FAILED,
        AUTHORIZED,
        PROHIBITED
    };

public:
    bool isConnectionAuthorized();

    void input_and_send_password();
    void input_and_send_name();

    client(boost::asio::io_context& io_context, boost::asio::ssl::context& context,
        const boost::asio::ip::tcp::resolver::results_type& endpoints);

    void write(const message& msg);

private:
    void input_and_send(message& msg);
    static bool verify_certificate(bool preverified, boost::asio::ssl::verify_context&);
    void notify_state(state new_state);
    void connect(const boost::asio::ip::tcp::resolver::results_type& endpoints);
    void handshake();
    void do_read_header();
    void do_read_body();
    void do_write();

private:
    boost::asio::io_context& io_context_;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;
    message read_msg_;
    message_queue write_msgs_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
    state connection_state_;
};
