#pragma once
#include "Types.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <random>
#include <optional>

// ---------------------------------------------------------
// 1. Message Source
// ---------------------------------------------------------
class MessageSource {
private:
    uint32_t next_message_id;                           // An ID ranging from 1 to 1,000,000
    std::mt19937 rng;                                   // Modern random number generator (Mersenne Twister)
    std::uniform_int_distribution<int> size_dist;       // Size generation between 1 and 1000
    std::uniform_int_distribution<int> queue_dist;      // Queue selection between 1 and 5
    std::uniform_real_distribution<double> chance_dist; // Probability generation (e.g., 50% chance to create a message each iteration)

    // Helper function for generating the sequence of numbers (Payload) as required
    std::string generate_payload(uint32_t id, int size);

public:
    MessageSource();
    
    // The function called on each iteration of the time loop.
    // If a message is generated, it is split into fragments (up to 100 characters) which are returned in a vector,
    // and the out_target_queue reference is updated.
    // If no message is generated, an empty vector is returned.
    std::vector<Fragment> tick(int& out_target_queue); 
};

// ---------------------------------------------------------
// 2. Queue System & Round Robin
// ---------------------------------------------------------
class QueueSystem {
private:
    static const int NUM_QUEUES = 5;
    
    // usage of an array of 5 queues (std::queue)
    std::vector<std::queue<Fragment>> queues;
    
    // the pointer that remembers the current queue for Round Robin (values: 0 to 4)
    int current_rr_index; 

public:
    QueueSystem();

    // enqueuing a collection of fragments for a new message into the target queue (1 to 5)
    void enqueue_fragments(int queue_id, const std::vector<Fragment>& fragments);

    // dispatching a single fragment based on the Round Robin algorithm.
    // using std::optional to allow returning "nothing" (std::nullopt) if the selected queue is empty.
    std::optional<Fragment> dispatch_next();
};

// ---------------------------------------------------------
// 3. Internal helper structure for the receiver – managing the assembly of a single message
// ---------------------------------------------------------
struct ReassemblyBuffer {
    int total_fragments_expected = 0;
    int fragments_received = 0;
    std::vector<Fragment> parts; // Stores the fragments received so far for a specific message

};

// ---------------------------------------------------------
// 4. Message Receiving System (Receiver)
// ---------------------------------------------------------
class Receiver {
private:
    // Dictionary that groups all the fragments of messages that have not yet been fully received, mapped by the message_id
    std::unordered_map<uint32_t, ReassemblyBuffer> assembly_map;
    
    // Final output queue to which messages are added only after they have been fully reassembled (Reassembly)
    std::queue<Message> output_queue; 

    // Internal function that checks if all parts have been received for a specific message ID. If so, it assembles them and moves to the output queue.
    void attempt_reassembly(uint32_t message_id);

public:
    Receiver() = default;

// The function that receives a single fragment from the Round Robin system
    void receive_fragment(const Fragment& fragment);

    // At the end of the program (after the time loop), this function will print statistics about the messages that were received, their order, and their size.
    void print_statistics() const;
};