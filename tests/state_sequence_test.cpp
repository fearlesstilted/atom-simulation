#include "state_sequence.hpp"

#include <iostream>
#include <set>
#include <tuple>

int main()
{
    constexpr int stateCount = 55;
    quantum::ComplexState state{1, 0, 0, 1};
    std::set<std::tuple<int, int, int>> visited;

    for (int i = 0; i < stateCount; ++i) {
        if (!quantum::isValid(state)) {
            std::cerr << "FAIL: sequence produced invalid state\n";
            return 1;
        }
        visited.emplace(state.n, state.l, state.m);
        state = sequence::nextState(state);
    }

    if (visited.size() != stateCount) {
        std::cerr << "FAIL: sequence did not visit all states\n";
        return 1;
    }
    if (state.n != 1 || state.l != 0 || state.m != 0) {
        std::cerr << "FAIL: sequence did not wrap to 1s\n";
        return 1;
    }
    return 0;
}
