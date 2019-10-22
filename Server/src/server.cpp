#include "server.hpp"

using boost::asio::ip::tcp;

server::server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint)
    : acceptor_(io_context, endpoint)
    , context_(boost::asio::ssl::context::sslv23)
{
    context_.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::single_dh_use);
    context_.set_password_callback(get_password);
    context_.use_certificate_chain_file("server.crt");
    context_.use_private_key_file("server.key", boost::asio::ssl::context::pem);
    context_.use_tmp_dh_file("dh2048.pem");

    do_accept();
}

std::string server::get_password(std::size_t, boost::asio::ssl::context_base::password_purpose) { return ""; }

void server::do_accept()
{
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec)
        {
            std::make_shared<session>(std::move(socket), context_, room_)->start();
        }

        do_accept();
    });
}
