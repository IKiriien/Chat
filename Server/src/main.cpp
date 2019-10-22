#include "server.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <list>

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
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), std::atoi(argv[i]));
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
