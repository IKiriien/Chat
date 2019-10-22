#include "session.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

using boost::asio::ip::tcp;

session::session(tcp::socket socket, boost::asio::ssl::context& context, room& room)
    : socket_(std::move(socket), context)
    , room_(room)
    , authorized_(false)
{
}

void session::start() { do_handshake(); }

void session::deliver(const message& msg)
{
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
        do_write();
    }
}

void session::do_handshake()
{
    auto self(shared_from_this());
    socket_.async_handshake(
        boost::asio::ssl::stream_base::server, [this, self](const boost::system::error_code& error) {
            if (!error)
            {
                do_read_header();
            }
        });
}

void session::do_read_header()
{
    auto self(shared_from_this());
    async_read(socket_, boost::asio::buffer(read_msg_.data(), message::header_length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec && read_msg_.decode_header())
            {
                do_read_body();
            }
            else if (authorized_)
            {
                room_.leave(shared_from_this());
            }
        });
}

void session::do_read_body()
{
    auto self(shared_from_this());
    async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec)
            {
                if (!authorized_)
                {
                    if (read_msg_.message_type() == message::REGISTER
                        && strncmp(read_msg_.body(), pwd_, read_msg_.body_length()) == 0)
                    {
                        do_authorized();
                    }
                    else
                    {
                        do_prohibited();
                    }
                }
                else
                {
                    room_.deliver(read_msg_);
                    do_read_header();
                }
            }
            else if (authorized_)
            {
                room_.leave(shared_from_this());
            }
        });
}

void session::do_prohibited()
{
    message prohibited(message::PROHIBITED);
    prohibited.encode_header();

    async_write(socket_, boost::asio::buffer(prohibited.data(), prohibited.length()),
        [](boost::system::error_code, std::size_t) {});
}

void session::do_authorized()
{
    message authorized(message::AUTHORIZED);
    authorized.encode_header();

    auto self(shared_from_this());
    async_write(socket_, boost::asio::buffer(authorized.data(), authorized.length()),
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec)
            {
                room_.join(shared_from_this());
                authorized_ = true;
                do_read_header();
            }
            else if (authorized_)
            {
                room_.leave(shared_from_this());
            }
        });
}

void session::do_write()
{
    auto self(shared_from_this());
    async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec)
            {
                write_msgs_.pop_front();
                if (!write_msgs_.empty())
                {
                    do_write();
                }
            }
            else if (authorized_)
            {
                room_.leave(shared_from_this());
            }
        });
}
