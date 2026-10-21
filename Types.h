#pragma once
#include <string>
#include <cstdint>

// A structure representing the complete original message (before splitting or after reassembly)
struct Message {
    uint32_t id;             // Unique running ID between 1 and 1,000,000
    int target_queue;        // Target queue number, randomly generated between 1 and 5
    int size;                // Total message size, randomly generated between 1 and 1000
    std::string payload;     // The complete string containing the content (e.g., "1000100110021003")
};

// A structure representing a single fragment that passes through the queue system
struct Fragment {
    // --- header information (Header) that enables Reassembly ---
    uint32_t message_id;     // ID of the original message to which this fragment belongs
    int fragment_index;      // Position of the fragment in the sequence (e.g., 0 for the first part, 1 for the second, etc.)
    int total_fragments;     // Total number of fragments that make up the complete message

    // --- payload (Content) ---
    std::string payload;     // The fragment itself, whose size will not exceed 100 characters
};