#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>

class chat_message
{
public:
  enum { header_length = 5 };
  enum { max_body_length = 512 };

  enum type
  {
    REGISTER = '0',
    NAME,
    AUTHORIZED,
    PROHIBITED,
    MESSAGE
  };

  chat_message()
    : body_length_(0),
      message_type_(type::MESSAGE)
  {
  }

  chat_message(type message_type)
    : body_length_(0),
      message_type_(message_type)
  {
  }

  const char* data() const
  {
    return data_;
  }

  char* data()
  {
    return data_;
  }

  std::size_t length() const
  {
    return header_length + body_length_;
  }

  const char* body() const
  {
    return data_ + header_length;
  }

  char* body()
  {
    return data_ + header_length;
  }

  std::size_t body_length() const
  {
    return body_length_;
  }

  void body_length(std::size_t new_length)
  {
    body_length_ = new_length;
    if (body_length_ > max_body_length)
      body_length_ = max_body_length;
  }

  bool decode_header()
  {
    char header[header_length] = "";
    message_type_ = static_cast<type>(data_[0]);
    std::strncat(header, data_ + 1, header_length - 1);
    body_length_ = std::atoi(header);

    std::cout << "Decode header: " << data_[0] << header << std::endl;

    if (body_length_ > max_body_length)
    {
      body_length_ = 0;
      return false;
    }
    return true;
  }

  void encode_header()
  {
    char header[header_length + 1] = "";
    std::sprintf(header, "%c%4d", static_cast<char>(message_type_) ,static_cast<int>(body_length_));
    std::memcpy(data_, header, header_length);

    std::cout << "Encode header: " << header << std::endl;
  }

  type message_type()
  {
    return message_type_;
  }

private:
  char data_[header_length + max_body_length];
  std::size_t body_length_;
  type message_type_;
};

#endif // CHAT_MESSAGE_HPP