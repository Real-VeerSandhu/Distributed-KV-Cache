#pragma once

#include <cstddef>
#include <type_traits>
#include <vector>

namespace kvcache {

template <typename T>
class Span {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using pointer = T*;
    using reference = T&;
    using iterator = pointer;

    constexpr Span() noexcept = default;
    constexpr Span(pointer ptr, size_type count) noexcept : data_(ptr), size_(count) {}

    template <std::size_t N>
    constexpr explicit Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

    [[nodiscard]] constexpr pointer data() const noexcept { return data_; }
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr iterator begin() const noexcept { return data_; }
    [[nodiscard]] constexpr iterator end() const noexcept { return data_ + size_; }

    [[nodiscard]] constexpr reference operator[](size_type idx) const noexcept {
        return data_[idx];
    }

    [[nodiscard]] constexpr Span<T> subspan(size_type offset, size_type count) const noexcept {
        return Span<T>(data_ + offset, count);
    }

    [[nodiscard]] constexpr Span<T> subspan(size_type offset) const noexcept {
        return Span<T>(data_ + offset, size_ - offset);
    }

private:
    pointer data_ = nullptr;
    size_type size_ = 0;
};

template <typename T>
[[nodiscard]] Span<const T> make_span(const std::vector<T>& v) noexcept {
    return Span<const T>(v.data(), v.size());
}

template <typename T>
[[nodiscard]] Span<T> make_span(std::vector<T>& v) noexcept {
    return Span<T>(v.data(), v.size());
}

}  // namespace kvcache
