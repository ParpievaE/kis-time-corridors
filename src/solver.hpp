#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Artifact {
    int node{};
    double value{};
    std::optional<std::int64_t> window_start;
    std::optional<std::int64_t> window_end;
};

struct Corridor {
    int from{};
    int to{};
    std::int64_t duration{};
    std::optional<std::int64_t> deadline;
    std::vector<int> required_artifacts; // zero-based artifact indices
};

struct Problem {
    std::int64_t time_limit{};
    double time_cost{};
    int node_count{};
    int start_node{};
    std::vector<int> exits;
    std::vector<Corridor> corridors;
    std::vector<Artifact> artifacts;
};

struct Result {
    bool feasible{false};
    std::int64_t minimum_duration{};
    double maximum_profit{};
};

Result solve(const Problem& problem);