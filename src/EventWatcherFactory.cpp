#include "EventWatcherFactory.hpp"

#if defined(__linux__)
# include "EventWatcherEpoll.hpp"
#else
# include "EventWatcherPoll.hpp"
#endif

namespace eventhub {
    std::shared_ptr<EventWatcher> createEventWatcher() {
#if defined(__linux__)
        return std::make_shared<EventWatcherEpoll>();
#else
        return std::make_shared<EventWatcherPoll>();
#endif
    }
}
