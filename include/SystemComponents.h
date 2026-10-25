#pragma once
#include "Types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------
// 1. Message Source
// ---------------------------------------------------------

// Splits a message into fragments of at most config::MAX_FRAGMENT_SIZE characters.
[[nodiscard]] std::vector<Fragment> fragment_message(std::uint32_t id, int target_queue,
                                                     std::string_view payload);

class MessageSource {
public:
    // Pass a seed for reproducible runs; otherwise a random_device seed is used.
    explicit MessageSource(std::optional<std::uint32_t> seed = std::nullopt);

    // Called once per iteration. Returns a fragmented message with probability
    // config::MESSAGE_PROBABILITY, otherwise std::nullopt.
    [[nodiscard]] std::optional<GeneratedMessage> tick();

private:
    // Payload is the sequence id, id+1, id+2, ... concatenated, cut to `size` characters.
    [[nodiscard]] static std::string generate_payload(std::uint32_t id, int size);

    std::uint32_t next_message_id_{1};
    std::mt19937 rng_;
    std::bernoulli_distribution create_dist_{config::MESSAGE_PROBABILITY};
    std::uniform_int_distribution<int> size_dist_{config::MIN_MESSAGE_SIZE, config::MAX_MESSAGE_SIZE};
    std::uniform_int_distribution<int> queue_dist_{1, static_cast<int>(config::NUM_QUEUES)};
};

// ---------------------------------------------------------
// 2. Queue System & Round Robin
// ---------------------------------------------------------
class QueueSystem {
public:
    explicit QueueSystem(std::size_t capacity = config::QUEUE_CAPACITY) : capacity_(capacity) {}

    // Enqueues all fragments into queue `queue_id` (1..NUM_QUEUES), or none of them:
    // returns false (and counts a dropped message) if they don't fit or queue_id is invalid.
    bool enqueue_message(int queue_id, std::vector<Fragment>&& fragments);

    // Dispatches one fragment, round-robin starting after the last served queue and
    // skipping empty queues. Returns std::nullopt only if every queue is empty.
    [[nodiscard]] std::optional<Fragment> dispatch_next();

    [[nodiscard]] std::size_t dropped_messages() const { return dropped_messages_; }
    [[nodiscard]] std::size_t fragments_in_flight() const;

private:
    std::array<std::queue<Fragment>, config::NUM_QUEUES> queues_;
    std::size_t capacity_;
    std::size_t rr_index_{0};          // next queue to examine
    std::size_t dropped_messages_{0};
};

// ---------------------------------------------------------
// 3. Receiver (reassembly)
// ---------------------------------------------------------

// Per-message reassembly state: payloads only, indexed by fragment_index.
struct ReassemblyBuffer {
    std::vector<std::optional<std::string>> parts;
    std::size_t received{0};
    std::size_t total_bytes{0};
    int source_queue{0};
};

class Receiver {
public:
    // Accepts one fragment (in any order). Returns false and counts it as rejected if it is
    // malformed (bad index/total, inconsistent total, or a duplicate).
    bool receive_fragment(Fragment fragment);

    // Pops the next fully reassembled message from the output queue.
    [[nodiscard]] std::optional<Message> pop_output();

    [[nodiscard]] std::size_t pending_messages() const { return assembly_map_.size(); }
    [[nodiscard]] std::size_t rejected_fragments() const { return rejected_fragments_; }

private:
    std::unordered_map<std::uint32_t, ReassemblyBuffer> assembly_map_; //dict in python
    std::queue<Message> output_queue_;
    std::size_t rejected_fragments_{0};
};
