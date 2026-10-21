#include "SystemComponents.h"
#include <iostream>
#include <cmath>

// =========================================================
// 1. Implementing the Message Source
// =========================================================

MessageSource::MessageSource() 
    : next_message_id(1), 
      rng(std::random_device{}()), // Initialize the random number generator with a true random seed
      size_dist(1, 1000), 
      queue_dist(1, 5), 
      chance_dist(0.0, 1.0) 
{}

// Helper function for generating the sequence of numbers (Payload) as required
// For example, if the ID is 1000, the sequence will be "100010011002..." until we reach the desired length.
std::string MessageSource::generate_payload(uint32_t id, int size) {
    std::string payload = "";
    uint32_t current_num = id;
    while (payload.length() < (size_t)size) {
        payload += std::to_string(current_num);
        current_num++;
    }
    // Exact cut to the desired size (since the last addition might have exceeded the size)
    return payload.substr(0, size);
}

std::vector<Fragment> MessageSource::tick(int& out_target_queue) {
    std::vector<Fragment> generated_fragments;
    
    // 50% chance of creating a message in each iteration (can be adjusted to control load)
    if (chance_dist(rng) > 0.5) {
        return generated_fragments; // Returns an empty vector, no message created
    }

    // --- Creating a new message ---
    uint32_t current_id = next_message_id++;
    int msg_size = size_dist(rng);
    out_target_queue = queue_dist(rng);
    std::string full_payload = generate_payload(current_id, msg_size);

    // --- Fragmentation ---
    // Maximum size for each fragment is 100 characters.
    const int MAX_FRAG_SIZE = 100;
    
    // Calculate the total number of fragments (with ceiling)

    int total_fragments = std::ceil((double)msg_size / MAX_FRAG_SIZE);

    for (int i = 0; i < total_fragments; ++i) {
        Fragment frag;
        frag.message_id = current_id;
        frag.fragment_index = i;
        frag.total_fragments = total_fragments;
        
        // Cut substring from the original message
        int start_pos = i * MAX_FRAG_SIZE;
        int current_frag_size = std::min(MAX_FRAG_SIZE, msg_size - start_pos);
        frag.payload = full_payload.substr(start_pos, current_frag_size);
        
        generated_fragments.push_back(frag);
    }

    return generated_fragments;
}

// =========================================================
// 2. Implementing the Queue System & Round Robin
// =========================================================

QueueSystem::QueueSystem() : current_rr_index(0) {
    // Initialize an array of 5 empty queues
    queues.resize(NUM_QUEUES);
}

void QueueSystem::enqueue_fragments(int queue_id, const std::vector<Fragment>& fragments) {
    // The queue IDs generated are 1-5, but the index in the array in C++ is 0-4
    int actual_index = queue_id - 1;
    
    // Enqueuing all fragments into the appropriate queue (FIFO)
    for (const auto& frag : fragments) {
        queues[actual_index].push(frag);
    }
}

std::optional<Fragment> QueueSystem::dispatch_next() {
    std::optional<Fragment> dispatched_frag = std::nullopt;

    // Check the current queue in the Round Robin cycle
    if (!queues[current_rr_index].empty()) {
        // If there is something in it - dispatch a single fragment
        dispatched_frag = queues[current_rr_index].front();
        queues[current_rr_index].pop();
    }

    // Advance the pointer to the next queue in the round-robin cycle (0 to 4)
    current_rr_index = (current_rr_index + 1) % NUM_QUEUES;

    // Return the dispatched fragment (or nullopt if the queue was empty)
    return dispatched_frag;
}

// =========================================================
// 3. Implementing the Receiver System
// =========================================================

void Receiver::receive_fragment(const Fragment& fragment) {
    // If this is the first fragment received for this message, the map automatically creates a buffer for it.
    ReassemblyBuffer& buffer = assembly_map[fragment.message_id];
    
    // Defining the expected total number of parts (set once when the first fragment of the message arrives)
    if (buffer.total_fragments_expected == 0) {
        buffer.total_fragments_expected = fragment.total_fragments;
        // Preparing the array with the appropriate size so we can place each fragment in the correct position according to its index
        buffer.parts.resize(fragment.total_fragments);
    }

    // Storing the fragment in the correct position according to its original index
    // (Prevents issues if message parts arrive out of order due to Round Robin)
    buffer.parts[fragment.fragment_index] = fragment;
    buffer.fragments_received++;

    // Checking if we have collected all the parts
    attempt_reassembly(fragment.message_id);
}

void Receiver::attempt_reassembly(uint32_t message_id) {
    ReassemblyBuffer& buffer = assembly_map[message_id];
    
    // Checking if we have collected all the parts
    if (buffer.fragments_received == buffer.total_fragments_expected) {
        
        // Reassembly of the complete Payload
        std::string full_payload = "";
        for (const auto& frag : buffer.parts) {
            full_payload += frag.payload;
        }

        // Creating the reassembled source message (without target_queue since it has already reached its destination)
        Message complete_msg;
        complete_msg.id = message_id;
        complete_msg.size = full_payload.length();
        complete_msg.payload = full_payload;

        // Insertion into the final output queue
        output_queue.push(complete_msg);

        // Clearing the memory of the map - we have finished processing this message
        assembly_map.erase(message_id);
    }
}

void Receiver::print_statistics() const {
    std::cout << "\n===== Simulation Completion Report ====="<< std::endl;
    std::cout << "Total messages in output queue: " << output_queue.size() << std::endl;
    
    // In C++, you cannot iterate over an std::queue using a standard for-loop, so we will create a copy and empty it.
    std::queue<Message> temp_queue = output_queue;
    
    int order = 1;
    while (!temp_queue.empty()) {
        const Message& msg = temp_queue.front();
        std::cout << order << ". Message ID: "<< msg.id 
                  << " | Size in output queue: " << msg.size 
                  << " | Initial content: " << msg.payload.substr(0, 15) << "..." << std::endl;
        temp_queue.pop();
        order++;
    }
    std::cout << "=============================\n" << std::endl;
}