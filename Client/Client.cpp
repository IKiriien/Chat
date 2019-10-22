#include <cstdlib>
#include <cstring>
#include <functional>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "message.h"
#include <condition_variable>
#include <mutex>

using boost::asio::ip::tcp;

typedef std::deque<message> chat_message_queue;

class chat_client
{
  enum state {
    NONE,
    CONNECTED,
    FAILED,
    AUTHORIZED,
    PROHIBITED
  };

public:

  bool isConnectionAuthorized()
  {
    for (std::unique_lock<std::mutex> lock(mutex_); connection_state_ == NONE;)
    {
      cond_var_.wait(lock);
    }

    if (connection_state_ == CONNECTED)
    {
      std::cout << "Enter password: ";
      message msg(message::REGISTER);
      std::cin.getline(msg.body(), message::max_body_length + 1);
      msg.body_length(std::strlen(msg.body()));
      msg.encode_header();
      write(msg);
      
      for (std::unique_lock<std::mutex> lock(mutex_); connection_state_ == CONNECTED;)
      {
        cond_var_.wait(lock);
      }

      if (connection_state_ == PROHIBITED)
      {
        std::cout << "Connection prohibited by server!" << std::endl;
      }
    }
    else
    {
      std::cout << "Connect or handshake failed!" << std::endl;
    }

    return (connection_state_ == AUTHORIZED);
  }

  chat_client(boost::asio::io_context& io_context,
      boost::asio::ssl::context& context,
      const tcp::resolver::results_type& endpoints)
    : io_context_(io_context),
      socket_(io_context, context),
      connection_state_(NONE)
  {
    socket_.set_verify_mode(boost::asio::ssl::verify_peer);
    socket_.set_verify_callback(verify_certificate);

    connect(endpoints);
  }

  void write(const message& msg)
  {
    boost::asio::post(io_context_,
        [this, msg]()
        {
          bool write_in_progress = !write_msgs_.empty();
          write_msgs_.push_back(msg);
          if (!write_in_progress)
          {
            do_write();
          }
        });
  }

private:

  static bool verify_certificate(bool preverified,
      boost::asio::ssl::verify_context&)
  {
    return preverified;
  }
  
  void notify_state(state new_state)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    connection_state_ = new_state;
    cond_var_.notify_one();
  }

  void connect(const tcp::resolver::results_type& endpoints)
  {
    boost::asio::async_connect(socket_.lowest_layer(), endpoints,
        [this](const boost::system::error_code& error,
          const tcp::endpoint& /*endpoint*/)
        {
          if (!error)
          {
            handshake();
          }
          else
          {
            notify_state(FAILED);
          }
        });
  }

  void handshake()
  {
    socket_.async_handshake(boost::asio::ssl::stream_base::client,
        [this](const boost::system::error_code& error)
        {
          if (!error)
          {
            notify_state(CONNECTED);
            do_read_header();
          }
          else
          {
            notify_state(FAILED);
          }
        });
  }

  void do_read_header()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.data(), message::header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            std::cout << "Read header failed: " << ec.message() << "\n";
          }
        });
  }

  void do_read_body()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            if (read_msg_.message_type() == message::PROHIBITED)
            {
              notify_state(PROHIBITED);
            }
            else if(read_msg_.message_type() == message::AUTHORIZED)
            {
              notify_state(AUTHORIZED);
              do_read_header();
            }
            else
            {
              std::cout << "> ";
              std::cout.write(read_msg_.body(), read_msg_.body_length());
              std::cout << std::endl;
              do_read_header();
            }
          }
          else
          {
            std::cout << "Read body failed: " << ec.message() << "\n";
          }
        });
  }

  void do_write()
  {
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            std::cout << "Write failed: " << ec.message() << "\n";
          }
        });
  }

private:
  boost::asio::io_context& io_context_;
  boost::asio::ssl::stream<tcp::socket> socket_;
  message read_msg_;
  chat_message_queue write_msgs_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  state connection_state_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: chat_client <host> <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], argv[2]);

    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.load_verify_file("client.crt");

    chat_client c(io_context, ctx, endpoints);

    std::thread t([&io_context](){ io_context.run(); });

    if (c.isConnectionAuthorized())
    {
      char line[message::max_body_length + 1];
      while (std::cin.getline(line, message::max_body_length + 1))
      {
        message msg;
        msg.body_length(std::strlen(line));
        std::memcpy(msg.body(), line, msg.body_length());
        msg.encode_header();
        c.write(msg);
      }
    }
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

