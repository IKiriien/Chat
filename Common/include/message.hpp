#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

class message
{
public:
    static constexpr int header_length = 5;
    static constexpr int max_body_length = 512;

    enum class type
    {
        REGISTER = '0', // Easy to debug
        NAME,
        AUTHORIZED,
        PROHIBITED,
        MESSAGE,
        KICK
    };

    message();
    explicit message(type message_type);

    const char* data() const;
    char* data();

    std::size_t length() const;

    const char* body() const;
    char* body();

    std::size_t body_length() const;
    void body_length(std::size_t new_length);

    bool decode_header();
    void encode_header();

    type message_type() const;

private:
    char data_[header_length + max_body_length];
    std::size_t body_length_;
    type message_type_;
};
