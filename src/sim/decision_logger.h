#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "sim/decision.h"

namespace kvcache::sim {

class DecisionLogger {
public:
    virtual ~DecisionLogger() = default;

    DecisionLogger(const DecisionLogger&) = delete;
    DecisionLogger& operator=(const DecisionLogger&) = delete;

    virtual void log(const Decision& decision) = 0;

protected:
    DecisionLogger() = default;
};

class NullDecisionLogger : public DecisionLogger {
public:
    NullDecisionLogger() = default;
    void log(const Decision&) override {}
};

class InMemoryDecisionLogger : public DecisionLogger {
public:
    InMemoryDecisionLogger() = default;

    void log(const Decision& decision) override;

    [[nodiscard]] const std::vector<Decision>& decisions() const noexcept;
    [[nodiscard]] std::size_t count() const noexcept;
    void clear() noexcept;

    template <typename T>
    [[nodiscard]] std::vector<T> decisionsOfType() const {
        std::vector<T> result;
        for (const auto& d : decisions_) {
            if (const auto* ptr = std::get_if<T>(&d)) {
                result.push_back(*ptr);
            }
        }
        return result;
    }

private:
    std::vector<Decision> decisions_;
};

class JsonlDecisionLogger : public DecisionLogger {
public:
    explicit JsonlDecisionLogger(std::ostream& out) noexcept;
    explicit JsonlDecisionLogger(const std::string& path);

    ~JsonlDecisionLogger() override;

    JsonlDecisionLogger(const JsonlDecisionLogger&) = delete;
    JsonlDecisionLogger& operator=(const JsonlDecisionLogger&) = delete;
    JsonlDecisionLogger(JsonlDecisionLogger&&) = delete;
    JsonlDecisionLogger& operator=(JsonlDecisionLogger&&) = delete;

    void log(const Decision& decision) override;
    void flush();

private:
    std::unique_ptr<std::ofstream> owned_file_;
    std::ostream& out_;
};

}  // namespace kvcache::sim
