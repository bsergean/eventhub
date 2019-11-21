#include "EventWatcherEpoll.hpp"

namespace eventhub {
    EventWatcherEpoll::EventWatcherEpoll()
        : EventWatcher(), _epoll_fd(-1)
    {
      _epoll_fd = epoll_create1(0);
    }
    
    EventWatcherEpoll::~EventWatcherEpoll()
    {
      if (_epoll_fd != -1) {
        close(_epoll_fd);
      }
    }
}
