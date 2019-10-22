#pragma once

#include "room.hpp"
#include "session.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

class server
{
public:
    server(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint);

private:
    static std::string get_password(std::size_t, boost::asio::ssl::context_base::password_purpose);

    void do_accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ssl::context context_;
    room room_;
};
