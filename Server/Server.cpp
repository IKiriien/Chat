#include "message.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>

using boost::asio::ip::tcp;

//----------------------------------------------------------------------

using message_queue = std::deque<message>;

//----------------------------------------------------------------------

class participant
{
public:
    virtual ~participant() {}
    virtual void deliver(const message& msg) = 0;
};

typedef std::shared_ptr<participant> participant_ptr;

//----------------------------------------------------------------------

class room
{
public:
    void join(participant_ptr participant)
    {
        participants_.insert(participant);
        for (auto msg : recent_msgs_)
            participant->deliver(msg);
    }

    void leave(participant_ptr participant) { participants_.erase(participant); }

    void deliver(const message& msg)
    {
        recent_msgs_.push_back(msg);
        while (recent_msgs_.size() > max_recent_msgs)
            recent_msgs_.pop_front();

        for (auto participant : participants_)
            participant->deliver(msg);
    }

private:
    std::set<participant_ptr> participants_;
    enum
    {
        max_recent_msgs = 100
    };
    message_queue recent_msgs_;
};

//----------------------------------------------------------------------

class session : public participant, public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket, boost::asio::ssl::context& context, room& room)
        : socket_(std::move(socket), context)
        , room_(room)
        , authorized_(false)
    {
    }

    void start() { do_handshake(); }

    void deliver(const message& msg)
    {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress)
        {
            do_write();
        }
    }

private:
    void do_handshake()
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

    void do_read_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), message::header_length),
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

    void do_read_body()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec)
                {
                    if (!authorized_)
                    {
                        if (read_msg_.message_type() == message::REGISTER
                            && strncmp(read_msg_.body(), "RingMediaServer", read_msg_.body_length()) == 0)
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

    void do_prohibited()
    {
        message prohibited(message::PROHIBITED);
        prohibited.encode_header();

        boost::asio::async_write(socket_, boost::asio::buffer(prohibited.data(), prohibited.length()),
            [](boost::system::error_code, std::size_t) {});
    }

    void do_authorized()
    {
        message authorized(message::AUTHORIZED);
        authorized.encode_header();

        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(authorized.data(), authorized.length()),
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

    void do_write()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
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

    boost::asio::ssl::stream<tcp::socket> socket_;
    room& room_;
    message read_msg_;
    message_queue write_msgs_;
    bool authorized_;
};

//----------------------------------------------------------------------

class server
{
public:
    server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint)
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

private:
    static std::string get_password(std::size_t, boost::asio::ssl::context_base::password_purpose) { return ""; }

    void do_accept()
    {
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec)
            {
                std::make_shared<session>(std::move(socket), context_, room_)->start();
            }

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
    boost::asio::ssl::context context_;
    room room_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: server <port> [<port> ...]\n";
            return 1;
        }

        boost::asio::io_context io_context;

        std::list<server> servers;
        for (int i = 1; i < argc; ++i)
        {
            tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
            servers.emplace_back(io_context, endpoint);
        }

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
