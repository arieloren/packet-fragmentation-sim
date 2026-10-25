#include "SystemComponents.h"

#include <algorithm>
#include <utility>

// =========================================================
// 1. Message Source
// =========================================================

std::vector<Fragment> fragment_message(std::uint32_t id, int target_queue, std::string_view payload) {
    constexpr std::size_t max_frag = config::MAX_FRAGMENT_SIZE;
    const std::size_t total = (payload.size() + max_frag - 1) / max_frag;

    std::vector<Fragment> fragments;
    fragments.reserve(total); // Pre-allocate capacity to prevent reallocations
    for (std::size_t i = 0; i < total; ++i) {
        const std::string_view slice = payload.substr(i * max_frag, max_frag);
        fragments.push_back(Fragment{id,
                                     static_cast<std::uint16_t>(i),
                                     static_cast<std::uint16_t>(total),
                                     static_cast<std::uint8_t>(target_queue),
                                     std::string(slice)});
    }
    return fragments;
}

MessageSource::MessageSource(std::optional<std::uint32_t> seed)
    : rng_(seed ? *seed : std::random_device{}()) {}

std::string MessageSource::generate_payload(std::uint32_t id, int size) {
    const auto target = static_cast<std::size_t>(size);
    std::string payload;
    payload.reserve(target + 10);  // the last number may overshoot by up to 10 digits
    for (std::uint32_t n = id; payload.size() < target; ++n) {
        payload += std::to_string(n);
    }
    payload.resize(target);
    return payload;
}

std::optional<GeneratedMessage> MessageSource::tick() {
    if (!create_dist_(rng_)) {
        return std::nullopt;
    }

    const std::uint32_t id = next_message_id_;
    next_message_id_ = (next_message_id_ % config::MAX_MESSAGE_ID) + 1;  // wraps 1,000,000 -> 1

    const int size = size_dist_(rng_);
    const int queue = queue_dist_(rng_);
    return GeneratedMessage{queue, fragment_message(id, queue, generate_payload(id, size))};
}

// =========================================================
// 2. Queue System & Round Robin
// =========================================================

bool QueueSystem::enqueue_message(int queue_id, std::vector<Fragment>&& fragments) {
    if (queue_id < 1 || queue_id > static_cast<int>(config::NUM_QUEUES)) {
        ++dropped_messages_;
        return false;
    }

    auto& queue = queues_[static_cast<std::size_t>(queue_id - 1)];
    if (queue.size() + fragments.size() > capacity_) {
        ++dropped_messages_;  // all-or-nothing: never leave a message half-admitted
        return false;
    }

    for (auto& frag : fragments) {
        queue.push(std::move(frag));
    }
    return true;
}

std::optional<Fragment> QueueSystem::dispatch_next() {
    for (std::size_t attempt = 0; attempt < queues_.size(); ++attempt) {
        const std::size_t idx = (rr_index_ + attempt) % queues_.size();
        auto& queue = queues_[idx];
        if (queue.empty()) {
            continue;
        }
        Fragment frag = std::move(queue.front());
        queue.pop();
        rr_index_ = (idx + 1) % queues_.size();
        return frag;
    }
    return std::nullopt;
}

std::size_t QueueSystem::fragments_in_flight() const {
    std::size_t total = 0;
    for (const auto& queue : queues_) {
        total += queue.size();
    }
    return total;
}

// =========================================================
// 3. Receiver
// =========================================================

bool Receiver::receive_fragment(Fragment fragment) {
    if (fragment.total_fragments == 0 || fragment.fragment_index >= fragment.total_fragments) {
        ++rejected_fragments_;
        return false;
    }

    auto it = assembly_map_.find(fragment.message_id);
    if (it == assembly_map_.end()) {
        ReassemblyBuffer buffer;
        buffer.parts.resize(fragment.total_fragments);
        buffer.source_queue = fragment.source_queue;
        it = assembly_map_.emplace(fragment.message_id, std::move(buffer)).first;
    }
    ReassemblyBuffer& buffer = it->second;

    if (buffer.parts.size() != fragment.total_fragments || buffer.parts[fragment.fragment_index]) {
        ++rejected_fragments_;  // inconsistent total, or duplicate fragment
        return false;
    }

    // Place by index, so arrival order doesn't matter.
    buffer.total_bytes += fragment.payload.size();
    buffer.parts[fragment.fragment_index] = std::move(fragment.payload);
    ++buffer.received;

    if (buffer.received == buffer.parts.size()) {
        std::string payload;
        payload.reserve(buffer.total_bytes);
        for (auto& part : buffer.parts) {
            payload += *part;
        }
        output_queue_.push(Message{it->first, buffer.source_queue, std::move(payload)});
        assembly_map_.erase(it);
    }
    return true;
}

std::optional<Message> Receiver::pop_output() {
    if (output_queue_.empty()) {
        return std::nullopt;
    }
    Message msg = std::move(output_queue_.front());
    output_queue_.pop();
    return msg;
}
