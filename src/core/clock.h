#pragma once

#include <cstdint>

namespace kvcache {

class Clock {
public:
    virtual ~Clock() = default;

    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;

    [[nodiscard]] virtual uint64_t nowNs() const = 0;

    virtual void advanceNs(uint64_t delta) = 0;

protected:
    Clock() = default;
};

class RealClock : public Clock {
public:
    RealClock() = default;

    [[nodiscard]] uint64_t nowNs() const override;

    void advanceNs(uint64_t delta) override;
};

class FakeClock : public Clock {
public:
    explicit FakeClock(uint64_t initial_ns = 0) noexcept;

    [[nodiscard]] uint64_t nowNs() const override;

    void advanceNs(uint64_t delta) override;

    void reset(uint64_t ns = 0) noexcept;

private:
    uint64_t current_ns_;
    // TODO(threading): make current_ns_ std::atomic<uint64_t> for multi-threaded use
};

}  // namespace kvcache
