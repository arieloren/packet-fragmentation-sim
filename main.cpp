#include "SystemComponents.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

int main(int argc, char* argv[]) {
    std::optional<std::uint32_t> seed;
    if (argc > 1) {
        try {
            seed = static_cast<std::uint32_t>(std::stoul(argv[1]));
        } catch (const std::exception&) {
            std::cerr << "Usage: " << argv[0] << " [seed]\n";
            return 1;
        }
    }

    std::cout << "Starting simulation...\n";

    MessageSource source(seed);
    QueueSystem queues;
    Receiver receiver;

    std::size_t generated = 0;
    std::size_t admitted = 0;

    for (int tick = 0; tick < config::TOTAL_TICKS; ++tick) {
        // Stage A: the source may generate a message.
        if (auto msg = source.tick()) {
            ++generated;
            if (queues.enqueue_message(msg->target_queue, std::move(msg->fragments))) {
                ++admitted;
            }
        }

        // Stage B: round-robin dispatch of a single fragment.
        // Stage C: hand it to the receiver.
        if (auto fragment = queues.dispatch_next()) {
            receiver.receive_fragment(std::move(*fragment));
        }
    }

    std::cout << "Simulation finished (" << config::TOTAL_TICKS << " iterations).\n";

    std::cout << "\n===== Output queue =====\n";
    std::size_t order = 0;
    while (auto msg = receiver.pop_output()) {
        std::cout << ++order << ". Message ID: " << msg->id
                  << " | Queue: " << msg->target_queue
                  << " | Size: " << msg->payload.size()
                  << " | Content: " << msg->payload.substr(0, 15) << "...\n";
    }

    std::cout << "\n===== Summary =====\n"
              << "Messages generated:           " << generated << '\n'
              << "Messages admitted to queues:  " << admitted << '\n'
              << "Messages dropped (overflow):  " << queues.dropped_messages() << '\n'
              << "Messages delivered:           " << order << '\n'
              << "Messages partially received:  " << receiver.pending_messages() << '\n'
              << "Fragments still queued:       " << queues.fragments_in_flight() << '\n'
              << "Fragments rejected:           " << receiver.rejected_fragments() << '\n';

    return 0;
}
