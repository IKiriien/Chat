#pragma once

#include "message.hpp"
#include "participant.hpp"

#include <deque>
#include <memory>
#include <set>

class room
{
    using participant_ptr = std::shared_ptr<participant>;

    static constexpr char kick_command[] = "@kick ";
    static constexpr char admin_user[] = "Admin";
    static constexpr char name_message_delimiter[] = "> ";

public:
    void join(participant_ptr participant);
    void leave(participant_ptr participant);
    void deliver(participant_ptr participant, const message& msg);

private:
    static constexpr int max_recent_msgs = 100;
    void deliver(const message& msg);

    std::set<participant_ptr> participants_;
    std::deque<message> recent_msgs_;
};
