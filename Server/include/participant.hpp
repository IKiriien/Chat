#pragma once

#include "message.hpp"
#include <string>

class participant
{
public:
    virtual ~participant() {}
    virtual void deliver(const message& msg) = 0;
    virtual std::string name() const = 0;
};
