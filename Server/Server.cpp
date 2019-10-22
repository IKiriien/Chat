#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "chat_message.hpp"

using boost::asio::ip::tcp;

//----------------------------------------------------------------------

using chat_message_queue = std::deque<chat_message>;

//----------------------------------------------------------------------

class chat_participant
{
public:
  virtual ~chat_participant() {}
  virtual void deliver(const chat_message& msg) = 0;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
  void join(chat_participant_ptr participant)
  {
    participants_.insert(participant);
    for (auto msg: recent_msgs_)
      participant->deliver(msg);
  }

  void leave(chat_participant_ptr participant)
  {
    participants_.erase(participant);
  }

  void deliver(const chat_message& msg)
  {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
      recent_msgs_.pop_front();

    for (auto participant: participants_)
      participant->deliver(msg);
  }

private:
  std::set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  chat_message_queue recent_msgs_;
};

//----------------------------------------------------------------------

class chat_session
  : public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
  chat_session(tcp::socket socket,
    boost::asio::ssl::context& context,
    chat_room& room)
    : socket_(std::move(socket), context),
      room_(room),
      authorized_(false)
  {
  }

  void start()
  {
    do_handshake();
  }

  void deliver(const chat_message& msg)
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
    socket_.async_handshake(boost::asio::ssl::stream_base::server, 
        [this, self](const boost::system::error_code& error)
        {
          if (!error)
          {
            do_read_header();
          }
        });
  }

  void do_read_header()
  {
    auto self(shared_from_this());
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.data(), chat_message::header_length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          // std::cout << "Finished reading header." << std::endl;

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
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            if (!authorized_)
            {
              // std::cout << "Password received: ";
              // std::cout.write(read_msg_.body(), read_msg_.body_length());
              // std::cout << std::endl;

              if (read_msg_.message_type() == chat_message::REGISTER &&
                strncmp(read_msg_.body(), "RingMediaServer", read_msg_.body_length()) == 0)
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
    chat_message prohibited(chat_message::PROHIBITED);
    prohibited.encode_header();

    boost::asio::async_write(socket_,
        boost::asio::buffer(prohibited.data(),
          prohibited.length()),
          [](boost::system::error_code, std::size_t){});
  }

  void do_authorized()
  {
    chat_message authorized(chat_message::AUTHORIZED);
    authorized.encode_header();

    auto self(shared_from_this());
    boost::asio::async_write(socket_,
        boost::asio::buffer(authorized.data(),
          authorized.length()),
          [this, self](boost::system::error_code ec, std::size_t)
          {
            if (!ec)
            {
              room_.join(shared_from_this());
              authorized_ = true;

              // std::cout << "Autorization granted! Start async reading!" << std::endl;

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
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
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
  chat_room& room_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
  bool authorized_;
};

//----------------------------------------------------------------------

class chat_server
{
public:
  chat_server(boost::asio::io_context& io_context,
      const tcp::endpoint& endpoint)
    : acceptor_(io_context, endpoint),
      context_(boost::asio::ssl::context::sslv23)
  {
    context_.set_options(
        boost::asio::ssl::context::default_workarounds
        | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::single_dh_use);
    context_.set_password_callback(get_password);
    context_.use_certificate_chain_file("server.crt");
    context_.use_private_key_file("server.key", boost::asio::ssl::context::pem);
    context_.use_tmp_dh_file("dh2048.pem");

    do_accept();
  }

private:
  static std::string get_password(std::size_t,
    boost::asio::ssl::context_base::password_purpose)
  {
    return "";
  }

  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<chat_session>(std::move(socket), context_, room_)->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
  boost::asio::ssl::context context_;
  chat_room room_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    if (argc < 2)
    {
      std::cerr << "Usage: chat_server <port> [<port> ...]\n";
      return 1;
    }

    boost::asio::io_context io_context;

    std::list<chat_server> servers;
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
