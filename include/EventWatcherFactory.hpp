#pragma once

#include <memory>

namespace eventhub {
    class EventWatcher;
    std::shared_ptr<EventWatcher> createEventWatcher();
} // namespace eventhub
