#include "SystemComponents.h"
#include <iostream>

int main() {
    std::cout << "מתחיל סימולציה..." << std::endl;

    // 1. אתחול שלושת הרכיבים הראשיים של המערכת
    MessageSource source;
    QueueSystem queues;
    Receiver receiver;

    // 2. הגדרת זמן הריצה (מספר סופי של איטרציות)
    const int TOTAL_TICKS = 1000; 

    // 3. לולאת הזמן המרכזית (Discrete-Time Simulation)
    for (int tick = 0; tick < TOTAL_TICKS; ++tick) {
        
        // שלב א': הפעלת מקור ההודעות
        int target_queue = 0;
        // מנסה להגריל הודעה. אם נוצרה, מחזיר את הפרגמנטים שלה
        std::vector<Fragment> new_fragments = source.tick(target_queue);
        
        // אם אכן נוצרו פרגמנטים (הוקצה target_queue תקין), נכניס אותם למערכת התורים
        if (!new_fragments.empty()) {
            queues.enqueue_fragments(target_queue, new_fragments);
        }

        // שלב ב': הפעלת הניתוב (Round Robin)
        // שולף פרגמנט בודד מאחד התורים על פי תורנות
        std::optional<Fragment> dispatched = queues.dispatch_next();

        // שלב ג': הפעלת המקלט (Receiver)
        // אם ה-Round Robin הצליח לשלוף פרגמנט (התור לא היה ריק), נעביר אותו למקלט
        if (dispatched.has_value()) {
            receiver.receive_fragment(dispatched.value());
        }
    }

    // 4. סיום התוכנית: הצגת הדוח
    std::cout << "הסימולציה הסתיימה (" << TOTAL_TICKS << " איטרציות)." << std::endl;
    receiver.print_statistics();

    return 0;
}