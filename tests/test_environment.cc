#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

TEST(Environment, GoogleTestWorks) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(Environment, NlohmannJsonParsesObject) {
    const auto j = nlohmann::json::parse(R"({"key": "value", "count": 42})");
    EXPECT_EQ(j["key"].get<std::string>(), "value");
    EXPECT_EQ(j["count"].get<int>(), 42);
}
