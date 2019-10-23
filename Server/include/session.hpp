#pragma once

#include "participant.hpp"
#include "room.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <deque>
#include <memory>
#include <string>

class session : public participant, public std::enable_shared_from_this<session>
{
    static constexpr char pwd_[] = "RingMediaServer"; // Better store hash here

public:
    session(boost::asio::ip::tcp::socket socket, boost::asio::ssl::context& context, room& room);

    void start();
    void deliver(const message& msg);
    virtual std::string name() const;

    virtual ~session() = default;

private:
    void do_handshake();
    void do_read_header();
    void do_read_body();
    void do_prohibited();
    void do_authorized();
    void do_write();

    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;
    room& room_;
    message read_msg_;
    std::deque<message> write_msgs_;
    bool authorized_;
    std::string name_;
};
