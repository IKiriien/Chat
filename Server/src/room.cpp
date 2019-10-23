#include "room.hpp"

void room::join(participant_ptr participant)
{
    participants_.insert(participant);
    for (auto msg : recent_msgs_)
        participant->deliver(msg);
}

void room::leave(participant_ptr participant) { participants_.erase(participant); }

void room::deliver(participant_ptr participant, const message& msg)
{
    if (msg.message_type() == message::type::MESSAGE)
    {
        std::string new_body = participant->name() + "> " + std::string(msg.body(), msg.body() + msg.body_length());
        message new_msg(message::type::MESSAGE);
        new_msg.body_length(new_body.length());
        std::memcpy(new_msg.body(), new_body.c_str(), new_msg.body_length());
        new_msg.encode_header();
        deliver(new_msg);
    }
    else
    {
        deliver(msg);
    }
}

void room::deliver(const message& msg)
{
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
        recent_msgs_.pop_front();

    for (auto participant : participants_)
        participant->deliver(msg);
}
