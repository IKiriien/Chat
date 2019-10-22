#include "client.hpp"

void client::input_and_send(message& msg)
{
    std::cin.getline(msg.body(), message::max_body_length + 1);
    msg.body_length(std::strlen(msg.body()));
    msg.encode_header();
    write(msg);
}

void client::input_and_send_password()
{
    std::cout << "Enter password: ";
    message msg(message::type::REGISTER);
    input_and_send(msg);
}

void client::input_and_send_name()
{
    std::cout << "Enter your nick name: ";
    message msg(message::type::NAME);
    input_and_send(msg);
}

bool client::isConnectionAuthorized()
{
    for (std::unique_lock<std::mutex> lock(mutex_); connection_state_ == NONE;)
    {
        cond_var_.wait(lock);
    }

    if (connection_state_ == FAILED)
    {
        std::cout << "Connect or handshake failed!" << std::endl;
        return false;
    }

    input_and_send_password();

    for (std::unique_lock<std::mutex> lock(mutex_); connection_state_ == CONNECTED;)
    {
        cond_var_.wait(lock);
    }

    if (connection_state_ == PROHIBITED)
    {
        std::cout << "Connection prohibited by server!" << std::endl;
        return false;
    }

    return true;
}

client::client(boost::asio::io_context& io_context, boost::asio::ssl::context& context,
    const boost::asio::ip::tcp::resolver::results_type& endpoints)
    : io_context_(io_context)
    , socket_(io_context, context)
    , connection_state_(NONE)
{
    socket_.set_verify_mode(boost::asio::ssl::verify_peer);
    socket_.set_verify_callback(verify_certificate);

    connect(endpoints);
}

void client::write(const message& msg)
{
    boost::asio::post(io_context_, [this, msg]() {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress)
        {
            do_write();
        }
    });
}

bool client::verify_certificate(bool preverified, boost::asio::ssl::verify_context&) { return preverified; }

void client::notify_state(state new_state)
{
    std::unique_lock<std::mutex> lock(mutex_);
    connection_state_ = new_state;
    cond_var_.notify_one();
}

void client::connect(const boost::asio::ip::tcp::resolver::results_type& endpoints)
{
    boost::asio::async_connect(socket_.lowest_layer(), endpoints,
        [this](const boost::system::error_code& error, const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
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

void client::handshake()
{
    socket_.async_handshake(boost::asio::ssl::stream_base::client, [this](const boost::system::error_code& error) {
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

void client::do_read_header()
{
    boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), message::header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
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

void client::do_read_body()
{
    boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec)
            {
                switch (read_msg_.message_type())
                {
                case message::type::PROHIBITED:
                    notify_state(PROHIBITED);
                    break;

                case message::type::AUTHORIZED:
                    notify_state(AUTHORIZED);
                    do_read_header();
                    break;

                case message::type::NAME:
                    std::cout.write(read_msg_.body(), read_msg_.body_length());
                    std::cout << " joined chat" << std::endl;
                    do_read_header();
                    break;

                case message::type::MESSAGE:
                    std::cout << "> ";
                    std::cout.write(read_msg_.body(), read_msg_.body_length());
                    std::cout << std::endl;
                    do_read_header();
                    break;

                default:
                    std::cout << "Received unexpected message from server!" << std::endl;
                    break;
                }
            }
            else
            {
                std::cout << "Read body failed: " << ec.message() << "\n";
            }
        });
}

void client::do_write()
{
    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
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
