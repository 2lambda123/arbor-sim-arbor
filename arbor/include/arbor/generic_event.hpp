#pragma once

#include <utility>
#include <type_traits>

// Generic accessors for event types used in `event_queue` and
// `event_stream`.
//
// 1. event_time(const Event&):
//
//    Returns ordered type (typically `time_type`) representing
//    the event time. Default implementation returns `e.time`
//    for an event `e`.
//
// 2. event_data(const Event&):
//
//    Returns the event _payload_, viz. the event data that
//    does not include (necessarily) the time or index. This
//    is used with `event_stream`.
//    Default implementation returns `e.data` for an event `e`.
//
// The type aliases event_time_type<Event> and event_data_type<Event>
// give the corresponding return types.
//
// The accessors act as customization points, in that they can be
// specialized for a particular event class. In order for ADL
// to work correctly across namespaces, the accessor functions
// should be brought into scope with a `using` declaration.
//
// Example use:
//
// template <typename Event>
// bool is_before(const Event& a, const Event& b) {
//     using ::arb::event_time;
//     return event_time(a)<event_time(b);
// }

namespace arb {

template <typename Event>
auto event_time(const Event& ev) {
    return ev.time;
}

template <typename Event>
auto event_data(const Event& ev) {
    return ev.data;
}

template <typename Event>
auto event_kind(const Event& ev) {
    return ev.kind;
}

struct event_time_less {
    template <typename T, typename Event, typename = std::enable_if_t<std::is_floating_point<T>::value>>
    bool operator() (T l, const Event& r) {
        return l<event_time(r);
    }

    template <typename T, typename Event, typename = std::enable_if_t<std::is_floating_point<T>::value>>
    bool operator() (const Event& l, T r) {
        return event_time(l)<r;
    }
};

namespace impl {
    // Wrap in `impl::` namespace to obtain correct ADL for return type.

    using ::arb::event_time;
    using ::arb::event_data;
    using ::arb::event_kind;

    template <typename Event>
    using event_time_type = decltype(event_time(std::declval<Event>()));

    template <typename Event>
    using event_data_type = decltype(event_data(std::declval<Event>()));

    template <typename Event>
    using event_kind_type = decltype(event_kind(std::declval<Event>()));
}

template <typename Event>
using event_time_type = impl::event_time_type<Event>;

template <typename Event>
using event_data_type = impl::event_data_type<Event>;

template <typename Event>
using event_kind_type = impl::event_kind_type<Event>;

} // namespace arb

