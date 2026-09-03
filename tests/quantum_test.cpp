#include "quantum.hpp"

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    using quantum::isValid;

    check(isValid({1, 0, 0, 1}), "hydrogen 1s is valid");
    check(isValid({8, 7, -7, 2}), "upper supported state is valid");
    check(!isValid({0, 0, 0, 1}), "n starts at one");
    check(!isValid({9, 0, 0, 1}), "n stops at eight");
    check(!isValid({2, 2, 0, 1}), "l must be below n");
    check(!isValid({2, 1, 2, 1}), "absolute m cannot exceed l");
    check(!isValid({1, 0, 0, 0}), "nuclear charge is positive");

    return failures == 0 ? 0 : 1;
}
