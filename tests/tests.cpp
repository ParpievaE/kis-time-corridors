#include "solver.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <cmath>

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("ТЕСТ НЕ ПРОЙДЕН: " + message);
    }

    std::cout << "[OK] " << message << '\n';
}

// Тест 1. Вход не соединён с единственным выходом.
void testDisconnectedGraph() {
    Problem p;
    p.time_limit = 10;
    p.time_cost = 1.0;
    p.node_count = 3;
    p.start_node = 0;
    p.exits = {2};

    p.corridors = {
        {0, 1, 1, std::nullopt, {}}
    };

    // Артефакт можно получить, но до выхода добраться нельзя.
    p.artifacts = {
        {0, 1.0, std::nullopt, std::nullopt}
    };

    const Result result = solve(p);
    check(!result.feasible, "1. Несвязный граф: выхода достичь нельзя");
}

// Тест 2. До выхода можно добраться, но превышается T_limit.
void testTimeLimitExceeded() {
    Problem p;
    p.time_limit = 5;
    p.time_cost = 1.0;
    p.node_count = 3;
    p.start_node = 0;
    p.exits = {2};

    p.corridors = {
        {0, 1, 4, std::nullopt, {}},
        {1, 2, 4, std::nullopt, {}}
    };

    // Артефакт находится на маршруте.
    p.artifacts = {
        {1, 1.0, std::nullopt, std::nullopt}
    };

    const Result result = solve(p);
    check(!result.feasible, "2. Превышен T_limit");
}

// Тест 3. Дедлайн коридора не позволяет добраться до выхода
// в пределах допустимого времени миссии.
void testCorridorDeadline() {
    Problem p;
    p.time_limit = 3;
    p.time_cost = 1.0;
    p.node_count = 2;
    p.start_node = 0;
    p.exits = {1};

    p.corridors = {
        {0, 1, 2, 0, {}}
    };

    // Артефакт доступен в начальной вершине.
    p.artifacts = {
        {0, 1.0, std::nullopt, std::nullopt}
    };

    const Result result = solve(p);
    check(!result.feasible, "3. Дедлайн коридора нарушить нельзя");
}

// Тест 4. Есть короткий путь без артефакта и более длинный путь
// через артефакт. Артефакт доступен только в момент 10,
// но попасть к нему в этот момент при заданном T_limit нельзя.
// Выход без артефакта запрещён дополнительным допущением.
void testArtifactWindowAndMandatoryCollection() {
    Problem p;
    p.time_limit = 4;
    p.time_cost = 1.0;
    p.node_count = 3;
    p.start_node = 0;
    p.exits = {2};

    p.corridors = {
        {0, 2, 1, std::nullopt, {}}, // короткий путь без артефакта
        {0, 1, 2, std::nullopt, {}},
        {1, 2, 1, std::nullopt, {}}
    };

    p.artifacts = {
        {1, 5.0, 10, 10}
    };

    const Result result = solve(p);
    check(!result.feasible,
          "4. Нельзя выйти без артефакта, а окно артефакта недостижимо");
}


int main() {
    try {
        testDisconnectedGraph();
        testTimeLimitExceeded();
        testCorridorDeadline();
        testArtifactWindowAndMandatoryCollection();

        std::cout << "\nВсе тесты пройдены.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
