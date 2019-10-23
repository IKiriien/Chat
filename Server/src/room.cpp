#include "room.hpp"
#include <algorithm>

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
        const std::string body(msg.body(), msg.body() + msg.body_length());
        if (auto iters
            = std::mismatch(std::begin(body), std::end(body), std::begin(kick_command), std::end(kick_command) - 1);
            (iters.second == (std::end(kick_command) - 1)) && (participant->name() == admin_user))
        {
            std::string name(iters.first, std::end(body));
            auto user_it = std::find_if(std::begin(participants_), std::end(participants_),
                [&name](const auto& participant) { return participant->name() == name; });
            if (user_it != std::end(participants_))
            {
                leave(*user_it);
                message kick_msg(message::type::KICK);
                kick_msg.encode_header();
                (*user_it)->deliver(kick_msg);
            }
        }
        else
        {
            std::string new_body = participant->name() + name_message_delimiter + body;
            message new_msg(message::type::MESSAGE);
            new_msg.body_length(new_body.length());
            std::memcpy(new_msg.body(), new_body.c_str(), new_msg.body_length());
            new_msg.encode_header();
            deliver(new_msg);
        }
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
