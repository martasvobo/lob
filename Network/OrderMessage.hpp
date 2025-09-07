#pragma once
#include <string>
#include <cstdint>

struct OrderMessage
{
    uint64_t id;
        char symbol[16]; // fixed-size symbol
    double price;
    uint32_t quantity;
    char side; // 'B' for buy, 'S' for sell
        OrderMessage() = default;
        OrderMessage(uint64_t id_, const char* sym, double price_, uint32_t qty_, char side_)
            : id(id_), price(price_), quantity(qty_), side(side_) {
            std::memset(symbol, 0, sizeof(symbol));
            std::strncpy(symbol, sym, sizeof(symbol) - 1);
        }
};
