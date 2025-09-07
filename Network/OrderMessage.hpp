#pragma once
#include <string>
#include <cstdint>

struct OrderMessage
{
    uint64_t id;
    std::string symbol;
    double price;
    uint32_t quantity;
    char side; // 'B' for buy, 'S' for sell
};
