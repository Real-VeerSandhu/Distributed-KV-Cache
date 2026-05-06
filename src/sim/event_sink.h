#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "sim/event.h"

namespace kvcache::sim {

class EventSink {
public:
    virtual ~EventSink() = default;

    EventSink(const EventSink&) = delete;
    EventSink& operator=(const EventSink&) = delete;

    virtual void record(const Event& event) = 0;

protected:
    EventSink() = default;
};

class NullEventSink : public EventSink {
public:
    NullEventSink() = default;
    void record(const Event&) override {}
};

class InMemoryEventSink : public EventSink {
public:
    InMemoryEventSink() = default;

    void record(const Event& event) override;

    [[nodiscard]] const std::vector<Event>& events() const noexcept;
    [[nodiscard]] std::size_t count() const noexcept;
    void clear() noexcept;

    template <typename T>
    [[nodiscard]] std::vector<T> eventsOfType() const {
        std::vector<T> result;
        for (const auto& e : events_) {
            if (const auto* ptr = std::get_if<T>(&e)) {
                result.push_back(*ptr);
            }
        }
        return result;
    }

private:
    std::vector<Event> events_;
    // TODO(threading): mutex around events_
};

class JsonlEventSink : public EventSink {
public:
    explicit JsonlEventSink(std::ostream& out) noexcept;
    explicit JsonlEventSink(const std::string& path);

    ~JsonlEventSink() override;

    JsonlEventSink(const JsonlEventSink&) = delete;
    JsonlEventSink& operator=(const JsonlEventSink&) = delete;
    JsonlEventSink(JsonlEventSink&&) = delete;
    JsonlEventSink& operator=(JsonlEventSink&&) = delete;

    void record(const Event& event) override;
    void flush();

private:
    std::unique_ptr<std::ofstream> owned_file_;
    std::ostream& out_;
};

}  // namespace kvcache::sim
