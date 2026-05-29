#include <gtest/gtest.h>
#include <vector>

TEST(Smoke, BuildSystemWorks) {
    EXPECT_EQ(2 + 2, 4);
}

TEST(Smoke, GTestLinksCorrectly) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(v.size(), 3u);
}