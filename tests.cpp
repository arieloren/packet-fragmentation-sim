#include "SystemComponents.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>

static int g_failures = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ++g_failures;                                                            \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #cond "\n"; \
        }                                                                            \
    } while (false)

static std::string make_payload(std::size_t n) {
    std::string s;
    for (std::size_t i = 0; s.size() < n; ++i) s += std::to_string(1000 + i);
    s.resize(n);
    return s;
}

static void test_fragmentation() {
    for (std::size_t size : {1u, 99u, 100u, 101u, 200u, 1000u}) {
        const std::string payload = make_payload(size);
        const auto frags = fragment_message(7, 3, payload);
        CHECK(frags.size() == (size + 99) / 100);
        std::string joined;
        for (std::size_t i = 0; i < frags.size(); ++i) {
            CHECK(frags[i].message_id == 7);
            CHECK(frags[i].fragment_index == i);
            CHECK(frags[i].total_fragments == frags.size());
            CHECK(frags[i].source_queue == 3);
            CHECK(frags[i].payload.size() <= 100);
            joined += frags[i].payload;
        }
        CHECK(joined == payload);
    }
}

static void test_reassembly_orders() {
    const std::string payload = make_payload(1000);

    for (int mode = 0; mode < 3; ++mode) {
        auto frags = fragment_message(42, 2, payload);
        if (mode == 1) std::reverse(frags.begin(), frags.end());
        if (mode == 2) std::shuffle(frags.begin(), frags.end(), std::mt19937(123));

        Receiver rx;
        for (std::size_t i = 0; i < frags.size(); ++i) {
            CHECK(!rx.pop_output());
            CHECK(rx.receive_fragment(std::move(frags[i])));
        }
        const auto msg = rx.pop_output();
        CHECK(msg.has_value());
        if (msg) {
            CHECK(msg->id == 42);
            CHECK(msg->target_queue == 2);
            CHECK(msg->payload == payload);
        }
        CHECK(rx.pending_messages() == 0);
    }
}

static void test_receiver_validation() {
    Receiver rx;
    auto frags = fragment_message(5, 1, make_payload(250));  // 3 fragments
    Fragment dup = frags[0];

    CHECK(rx.receive_fragment(std::move(frags[0])));
    CHECK(!rx.receive_fragment(std::move(dup)));      // duplicate
    CHECK(rx.rejected_fragments() == 1);
    CHECK(!rx.pop_output());                          // not complete yet
    CHECK(rx.pending_messages() == 1);

    Fragment bad_index = frags[1];
    bad_index.fragment_index = 3;
    CHECK(!rx.receive_fragment(std::move(bad_index)));

    Fragment bad_total = frags[1];
    bad_total.total_fragments = 4;
    CHECK(!rx.receive_fragment(std::move(bad_total)));

    Fragment zero_total = frags[1];
    zero_total.total_fragments = 0;
    CHECK(!rx.receive_fragment(std::move(zero_total)));
    CHECK(rx.rejected_fragments() == 4);

    CHECK(rx.receive_fragment(std::move(frags[1])));
    CHECK(rx.receive_fragment(std::move(frags[2])));
    CHECK(rx.pop_output().has_value());
}

static void test_round_robin() {
    QueueSystem qs;
    CHECK(!qs.dispatch_next());  // all empty

    // Queue 1: 2 fragments (id 1), queue 3: 2 fragments (id 3), queue 5: 1 fragment (id 5)
    CHECK(qs.enqueue_message(1, fragment_message(1, 1, make_payload(150))));
    CHECK(qs.enqueue_message(3, fragment_message(3, 3, make_payload(150))));
    CHECK(qs.enqueue_message(5, fragment_message(5, 5, make_payload(50))));
    CHECK(qs.fragments_in_flight() == 5);

    const std::uint32_t expected_ids[] = {1, 3, 5, 1, 3};  // empty queues are skipped
    for (std::uint32_t id : expected_ids) {
        auto f = qs.dispatch_next();
        CHECK(f.has_value());
        if (f) CHECK(f->message_id == id);
    }
    CHECK(!qs.dispatch_next());
    CHECK(qs.fragments_in_flight() == 0);
}

static void test_overflow_and_invalid_queue() {
    QueueSystem qs(5);
    CHECK(qs.enqueue_message(2, fragment_message(1, 2, make_payload(300))));   // 3 fragments
    CHECK(!qs.enqueue_message(2, fragment_message(2, 2, make_payload(300))));  // 3 more won't fit
    CHECK(qs.fragments_in_flight() == 3);                                      // atomic: nothing added
    CHECK(qs.dropped_messages() == 1);
    CHECK(qs.enqueue_message(2, fragment_message(3, 2, make_payload(200))));   // exactly fills it
    CHECK(qs.fragments_in_flight() == 5);

    CHECK(!qs.enqueue_message(0, fragment_message(4, 1, make_payload(10))));
    CHECK(!qs.enqueue_message(6, fragment_message(5, 1, make_payload(10))));
    CHECK(qs.dropped_messages() == 3);
}

static void test_source() {
    MessageSource a(99), b(99);
    std::size_t produced = 0;
    for (int i = 0; i < 200; ++i) {
        auto ma = a.tick();
        auto mb = b.tick();
        CHECK(ma.has_value() == mb.has_value());
        if (!ma || !mb) continue;
        ++produced;
        CHECK(ma->target_queue == mb->target_queue);
        CHECK(ma->target_queue >= 1 && ma->target_queue <= 5);
        CHECK(ma->fragments.size() == mb->fragments.size());
        CHECK(!ma->fragments.empty() && ma->fragments.size() <= 10);
        CHECK(ma->fragments[0].message_id == produced);
        CHECK(ma->fragments[0].payload == mb->fragments[0].payload);
        // payload starts with the message id itself
        CHECK(ma->fragments[0].payload.rfind(std::to_string(produced), 0) == 0);
    }
    CHECK(produced > 0);
}

int main() {
    test_fragmentation();
    test_reassembly_orders();
    test_receiver_validation();
    test_round_robin();
    test_overflow_and_invalid_queue();
    test_source();

    if (g_failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed.\n";
    return 1;
}
