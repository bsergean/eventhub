#include "EventWatcher.hpp"

namespace eventhub {
    class EventWatcherEpoll final : public EventWatcher
    {
    public:
        EventWatcherEpoll();
        ~EventWatcherEpoll();

    private:
      int _epoll_fd;
    };
}
