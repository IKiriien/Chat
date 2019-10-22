#pragma once

#include "message.hpp"

class participant
{
public:
    virtual ~participant() {}
    virtual void deliver(const message& msg) = 0;
};
