#include "client.hpp"

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: client <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        boost::asio::ip::tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);

        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
        ctx.load_verify_file("client.crt");

        client c(io_context, ctx, endpoints);

        std::thread t([&io_context]() { io_context.run(); });

        if (c.is_connection_authorized())
        {
            c.input_and_send_name();

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
