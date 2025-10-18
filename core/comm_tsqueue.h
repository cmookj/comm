//
//  comm_tsqueue.h
//

#ifndef COMM_TSQUEUE_H
#define COMM_TSQUEUE_H

#include "core/comm_log.h"

namespace gpw {
namespace net {

// Thread safe queue
template <typename T> class tsqueue {
  public:
    tsqueue ()                  = default;
    tsqueue (const tsqueue<T>&) = delete;  // Prevent copying
    virtual ~tsqueue () { clear(); }

    // Returns and maintains item at the front of Queue
    const T&
    front () {
        // Note
        //  The class `std::scoped_lock` is a mutex wrapper that provides a convenient
        //  RAII-style mechanism for owning one or more mutexes for the duration of
        //  a scoped block.
        //  (Defined in header <mutex>, since C++17)
        std::scoped_lock lock (_mutex);
        return _deque.front();
    }

    // Returns and maintains item at the back of Queue
    const T&
    back () {
        std::scoped_lock lock (_mutex);
        return _deque.back();
    }

    // Adds an item to the back of Queue
    void
    push_back (const T& item) {
        std::scoped_lock lock (_mutex);
        _deque.emplace_back (std::move (item));

        std::unique_lock<std::mutex> ul (_mutex_blocking);
        _blocking.notify_one();
    }

    // Adds an item to the front of Queue
    void
    push_front (const T& item) {
        std::scoped_lock lock (_mutex);
        _deque.emplace_front (std::move (item));

        std::unique_lock<std::mutex> ul (_mutex_blocking);
        _blocking.notify_one();
    }

    // Returns true if Queue has no items
    bool
    empty () {
        std::scoped_lock lock (_mutex);
        return _deque.empty();
    }

    // Returns the number of items in Queue
    std::size_t
    count () {
        std::scoped_lock lock (_mutex);
        return _deque.size();
    }

    // Clears Queue
    void
    clear () {
        std::scoped_lock lock (_mutex);
        _deque.clear();
    }

    // Removes and returns an item from the front of Queue
    T
    pop_front () {
        std::scoped_lock lock (_mutex);
        auto             t = std::move (_deque.front());
        _deque.pop_front();  // Note that this call destroys the front most element
                             // immediately, without returning it.
        return t;
    }

    // Removes and returns an item from the back of Queue
    T
    pop_back () {
        std::scoped_lock lock (_mutex);
        auto             t = std::move (_deque.back());
        _deque.pop_back();
        return t;
    }

    void
    wait () {
        while (empty() && _keep_waiting) {
            std::unique_lock<std::mutex> ul (_mutex_blocking);
            _blocking.wait (ul);
        }
    }

    void
    release_wait () {
        _keep_waiting = false;
        _blocking.notify_one();
    }

  protected:
    std::mutex    _mutex;
    std::deque<T> _deque;

    std::condition_variable _blocking;
    std::mutex              _mutex_blocking;

    bool _keep_waiting = true;
};

}  // namespace net
}  // namespace gpw

#endif
