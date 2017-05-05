#pragma once

#include <memory>
#include <typeinfo>
#include <type_traits>

#include <iostream> // TODO: remove

#include <util/meta.hpp>

// Partial implementation of std::any from C++17 standard.
//      http://en.cppreference.com/w/cpp/utility/any
//
// Implements a standard-compliant subset of the full interface.
//
// - Does not avoid dynamic allocation of small objects.
// - Does not implement the in_place_type<T> constructors from the standard.
// - Does not implement the in_place_type<T> constructors from the standard.
//


namespace nest {
namespace mc {
namespace util {

// Defines a type of object to be thrown by the value-returning forms of
// util::any_cast on failure.
//      http://en.cppreference.com/w/cpp/utility/any/bad_any_cast
class bad_any_cast: public std::bad_cast {
public:
    const char* what() const noexcept override {
        return "bad any cast";
    }
};

class any {
public:
    constexpr any() = default;

    any(const any& other):
        state_(other.state_->copy())
    {}

    any(any&& other) noexcept {
        std::swap(other.state_, state_);
    }

    template <
        typename T,
        typename = typename
            util::enable_if_t<!std::is_same<util::decay_t<T>, any>::value>
    >
    any(T&& other)
    {
        using contained_type = util::decay_t<T>;
        static_assert(
            std::is_copy_constructible<contained_type>::value,
            "Type of contained object stored in any must satisfy the CopyConstructible requirements.");

        state_.reset(new model<contained_type>(std::forward<T>(other)));
    }

    any& operator=(const any& other) {
        state_.reset(other.state_->copy());
        return *this;
    }

    any& operator=(any&& other) noexcept {
        swap(other);
        return *this;
    }

    template <
        typename T,
        typename = typename
            util::enable_if_t<!std::is_same<util::decay_t<T>, any>::value>
    >
    any& operator=(T&& other) {
        using contained_type = util::decay_t<T>;

        static_assert(
            std::is_copy_constructible<contained_type>::value,
            "Type of contained object stored in any must satisfy the CopyConstructible requirements.");

        state_.reset(new model<contained_type>(std::forward<T>(other)));
        return *this;
    }

    void reset() noexcept {
        state_.reset(nullptr);
    }

    void swap(any& other) noexcept {
        std::swap(other.state_, state_);
    }

    bool has_value() const noexcept {
        return (bool)state_;
    }

    const std::type_info& type() const noexcept {
        return state_->type();
    }

private:

    struct interface {
        virtual ~interface() = default;
        virtual const std::type_info& type() = 0;
        virtual interface* copy() = 0;
        virtual void* pointer() = 0;
        virtual const void* pointer() const = 0;
    };

    template <typename T>
    struct model: public interface {
        ~model() = default;

        interface* copy() override {
            return new model<T>(*this);
        }

        const std::type_info& type() override {
            return typeid(T);
        }

        model(const T& other):
            value(other)
        {}

        model(T&& other):
            value(std::move(other))
        {}

        void* pointer() {
            return &value;
        }

        const void* pointer() const {
            return &value;
        }

        T value;
    };

    std::unique_ptr<interface> state_;

protected:

    template <typename T>
    friend const T* any_cast(const any* operand);

    template <typename T>
    friend T* any_cast(any* operand);

    template <typename T>
    T* unsafe_cast() {
        return reinterpret_cast<T*>(state_->pointer());
    }

    template <typename T>
    const T* unsafe_cast() const {
        return reinterpret_cast<T*>(state_->pointer());
    }
};

namespace impl {
    template <typename T>
    using any_cast_remove_qual = typename
        std::remove_cv<typename std::remove_reference<T>::type>::type;

    template <typename ValueType, typename U=any_cast_remove_qual<ValueType>>
    using test_any_cast_constructable = typename
        std::conditional<
               std::is_constructible<ValueType, const U&>::value
            && std::is_constructible<ValueType, U&>::value
            && std::is_constructible<ValueType, U>::value,
            std::true_type,
            std::false_type>::type;

    template<typename T>
    inline T move_else_copy(any_cast_remove_qual<T>* p, std::true_type) {
        return std::move(*p);
    }

    template<typename T>
    inline T move_else_copy(any_cast_remove_qual<T>* p, std::false_type) {
        return *p;
    }
} // namespace impl

// If operand is not a null pointer, and the typeid of the requested T matches
// that of the contents of operand, a pointer to the value contained by operand,
// otherwise a null pointer.
template<class T>
const T* any_cast(const any* operand) {
    static_assert(
        impl::test_any_cast_constructable<T>::value,
        "any_cast requires ");

    if (operand==nullptr || !operand->has_value()) {
        return nullptr;
    }

    if (operand->type() != typeid(T)) {
        throw bad_any_cast();
    }

    return operand->unsafe_cast<T>();
}

// If operand is not a null pointer, and the typeid of the requested T matches
// that of the contents of operand, a pointer to the value contained by operand,
// otherwise a null pointer.
template<class T>
T* any_cast(any* operand) {
    static_assert(
        impl::test_any_cast_constructable<T>::value,
        "any_cast can't construct the desired type");

    if (operand==nullptr || !operand->has_value()) {
        return nullptr;
    }

    if (operand->type() != typeid(T)) {
        throw bad_any_cast();
    }

    return operand->unsafe_cast<T>();
}

template<class T>
T any_cast(const any& operand) {
    using U = impl::any_cast_remove_qual<T>;
    auto ptr = any_cast<U>(&operand);
    if (ptr==nullptr) {
        throw bad_any_cast();
    }
    return static_cast<T>(*ptr);
}

template<class T>
T any_cast(any& operand) {
    using U = impl::any_cast_remove_qual<T>;
    auto ptr = any_cast<U>(&operand);
    if (ptr==nullptr) {
        throw bad_any_cast();
    }
    return static_cast<T>(*ptr);
}

template<class T>
T any_cast(any&& operand) {
    using U = impl::any_cast_remove_qual<T>;
    using do_move = typename
        std::conditional<
            std::is_move_constructible<T>::value
            & !std::is_lvalue_reference<T>::value,
            std::true_type, std::false_type>::type;

    auto ptr = any_cast<U>(&operand);
    if (ptr==nullptr) {
        throw bad_any_cast();
    }
    return impl::move_else_copy<T>(static_cast<T*>(ptr), do_move());
}

} // namespace util
} // namespace mc
} // namespace nest
