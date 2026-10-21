# Packet Fragmentation Simulation

A discrete-time simulation of a message source, five bounded queues with round-robin dispatch, and a receiver that reassembles fragmented messages (C++17).

## Flow

Each iteration of the time loop runs every component once:

1. **MessageSource** – with probability 0.5 creates a message (size 1–1000, target queue 1–5, running ID 1–1,000,000). The payload is `id, id+1, id+2, ...` concatenated and cut to the size (e.g. `"1000100110021003..."`). The message is split into fragments of at most 100 characters.
2. **QueueSystem** – five bounded queues (capacity in fragments, `config::QUEUE_CAPACITY`). A message is admitted all-or-nothing; if its fragments don't fit, the whole message is dropped and counted. Each iteration exactly one fragment is dispatched, round-robin (1 → 2 → … → 5 → 1), skipping empty queues.
3. **Receiver** – places each fragment by index and, once all arrive, joins them into the original message and pushes it to the output queue.

At the end, the output queue is drained and the order, ID, target queue and size of each message is printed, followed by a summary (generated / dropped / delivered / pending / rejected).

## Fragment header (metadata)

| Field | Use |
|---|---|
| `message_id` | Grouping key – which message the fragment belongs to |
| `fragment_index` | 0-based position; the receiver stores the part in that slot, so out-of-order arrival is harmless |
| `total_fragments` | Tells the receiver when the message is complete, and is validated for consistency across fragments |
| `source_queue` | The target queue, restored onto the reassembled message |

The receiver rejects (and counts) malformed fragments: `total_fragments == 0`, `fragment_index >= total_fragments`, an inconsistent total, or a duplicate fragment.

## Build, run, test

```
cmake -S . -B build
cmake --build build --config Release
build/Release/packet_sim [seed]        # optional seed gives reproducible runs
ctest --test-dir build -C Release --output-on-failure
```
