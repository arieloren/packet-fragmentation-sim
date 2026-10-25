#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace config {
inline constexpr std::size_t NUM_QUEUES = 5;
inline constexpr std::size_t MAX_FRAGMENT_SIZE = 100;     // max payload characters per fragment
inline constexpr int MIN_MESSAGE_SIZE = 1;
inline constexpr int MAX_MESSAGE_SIZE = 1000;
inline constexpr std::uint32_t MAX_MESSAGE_ID = 1'000'000;
inline constexpr std::size_t QUEUE_CAPACITY = 50;         // fragments per queue
inline constexpr double MESSAGE_PROBABILITY = 0.5;        // chance of a new message per tick
inline constexpr int TOTAL_TICKS = 1000;
}  // namespace config

// A complete message (as reassembled by the receiver).
struct Message {
    std::uint32_t id{};
    int target_queue{};      // 1..NUM_QUEUES
    std::string payload;
};

// A single fragment travelling through the queue system.
struct Fragment {
    // --- header (metadata that enables reassembly) ---
    std::uint32_t message_id{};          // grouping key: which message this belongs to
    std::uint16_t fragment_index{};      // 0-based position: placement + out-of-order safety
    std::uint16_t total_fragments{};     // completion detection + consistency validation
    std::uint8_t source_queue{};         // 1..NUM_QUEUES, restored onto the reassembled message

    // --- body ---
    std::string payload;                 // at most config::MAX_FRAGMENT_SIZE characters
};

// Output of the message source: the target queue and the already-fragmented message.
struct GeneratedMessage {
    int target_queue{};
    std::vector<Fragment> fragments;
};
