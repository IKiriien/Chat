#pragma once

#include "message.hpp"
#include "participant.hpp"

#include <deque>
#include <memory>
#include <set>

class room
{
    using participant_ptr = std::shared_ptr<participant>;

public:
    void join(participant_ptr participant);
    void leave(participant_ptr participant);
    void deliver(const message& msg);

private:
    static constexpr int max_recent_msgs = 100;

    std::set<participant_ptr> participants_;
    std::deque<message> recent_msgs_;
};
