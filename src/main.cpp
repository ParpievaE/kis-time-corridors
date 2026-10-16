#include "solver.hpp"

#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// Преобразует текстовое значение времени в целое число.
// Если во входных данных вместо времени стоит «-», это означает,
// что соответствующее ограничение не задано.
std::optional<std::int64_t> read_optional_time(const std::string& token) {
    if (token == "-") {
        return std::nullopt;
    }
    std::size_t consumed = 0;
    long long value = std::stoll(token, &consumed);
    if (consumed != token.size()) {
        throw std::runtime_error("Некорректное значение времени: " + token);
    }
    return static_cast<std::int64_t>(value);
}

}

int main() {
    // Вся программа работает в два этапа:
    // 1. Считывает и проверяет описание задачи из стандартного ввода.
    // 2. Передаёт описание алгоритму solve и печатает найденный результат.
    // Ошибки входных данных выводятся отдельно, в поток стандартных ошибок.
    try {
        // Объект problem хранит все исходные данные задачи: вершины,
        // коридоры, артефакты, выходы и ограничения по времени.
        Problem problem;
        int corridor_count = 0;
        int artifact_count = 0;
        int exit_count = 0;

        // Сначала читаем общие параметры задачи:
        // T_lim - максимально допустимая длительность миссии;
        // T_cost - стоимость одной единицы потраченного времени.
        // Следующая строка содержит количества: N - вершин, M - коридоров,
        // K - артефактов, P - выходов. Затем читаются стартовая вершина и выходы.
        if (!(std::cin >> problem.time_limit >> problem.time_cost)) {
            std::cerr << "Ошибка ввода: ожидались значения T_lim и T_cost.\n";
            return 1;
        }
        if (!(std::cin >> problem.node_count >> corridor_count
                      >> artifact_count >> exit_count)) {
            std::cerr << "Ошибка ввода: ожидались значения N M K P.\n";
            return 1;
        }
        if (corridor_count < 0 || artifact_count < 0 || exit_count < 0) {
            throw std::runtime_error("Количество коридоров, предметов и выходов не может быть отрицательным.");
        }
        if (!(std::cin >> problem.start_node)) {
            throw std::runtime_error("Не указана начальная вершина.");
        }

        // Заранее выделяем место под список выходов и считываем их номера.
        // Вершины во входном файле нумеруются с нуля.
        problem.exits.resize(static_cast<std::size_t>(exit_count));
        for (int& exit_node : problem.exits) {
            if (!(std::cin >> exit_node)) {
                throw std::runtime_error("Не указан идентификатор выходной вершины.");
            }
        }

        // Далее считываем каждый коридор. В строке указаны:
        // начальная и конечная вершины, длительность, дедлайн,
        // количество требований и номера необходимых артефактов.
        // «-» вместо дедлайна означает, что ограничения по времени нет.
        // В файле номера артефактов начинаются с 1, при чтении
        // переводим их в индексы с нуля.
        problem.corridors.reserve(static_cast<std::size_t>(corridor_count));
        for (int i = 0; i < corridor_count; ++i) {
            Corridor edge;
            std::string deadline_token;
            int requirement_count = 0;

            if (!(std::cin >> edge.from >> edge.to >> edge.duration
                          >> deadline_token >> requirement_count)) {
                throw std::runtime_error("Некорректная строка коридора.");
            }
            if (requirement_count < 0) {
                throw std::runtime_error("Количество требований коридора не может быть отрицательным.");
            }
            edge.deadline = read_optional_time(deadline_token);
            edge.required_artifacts.reserve(
                static_cast<std::size_t>(requirement_count));

            for (int j = 0; j < requirement_count; ++j) {
                int artifact_id = 0;
                if (!(std::cin >> artifact_id)) {
                    throw std::runtime_error("Не указан идентификатор требуемого предмета.");
                }
                edge.required_artifacts.push_back(artifact_id - 1);
            }
            problem.corridors.push_back(std::move(edge));
        }

        // Теперь считываем артефакты: вершину, в которой он находится,
        // ценность и границы временного окна доступности.
        // Если указано «* *», артефакт доступен при любом времени посещения.
        // Если заданы две границы, обе включаются в окно доступности.
        problem.artifacts.reserve(static_cast<std::size_t>(artifact_count));
        for (int i = 0; i < artifact_count; ++i) {
            Artifact artifact;
            std::string window_start_token;
            std::string window_end_token;

            if (!(std::cin >> artifact.node >> artifact.value
                          >> window_start_token >> window_end_token)) {
                throw std::runtime_error("Некорректная строка предмета.");
            }

            if (window_start_token == "*" && window_end_token == "*") {
                artifact.window_start = std::nullopt;
                artifact.window_end = std::nullopt;
            } else if (window_start_token != "*" && window_end_token != "*") {
                artifact.window_start = read_optional_time(window_start_token);
                artifact.window_end = read_optional_time(window_end_token);
                if (!artifact.window_start || !artifact.window_end) {
                    throw std::runtime_error(
                        "Укажите две целочисленные границы временного окна либо «* *».");
                }
            } else {
                throw std::runtime_error(
                    "Временное окно предмета должно содержать две целочисленные границы либо «* *».");
            }

            problem.artifacts.push_back(artifact);
        }

        // Запускаем решение задачи. Алгоритм отдельно определяет,
        // существует ли допустимая миссия, минимальную длительность
        // среди допустимых миссий и максимальную выгоду.
        const Result result = solve(problem);

        // Если допустимой миссии нет, выводим понятное сообщение на русском.
        // Для числовых показателей используем «none», поскольку значения
        // минимального времени и максимальной выгоды в этом случае не определены.
        if (!result.feasible) {
            std::cout << "Миссия НЕ выполнима\n";
            std::cout << "Минимальное время: none\n";
            std::cout << "Максимальная выгода: none\n";
            return 0;
        }

        // Если миссия выполнима, выводим найденные показатели.
        // Не задаём фиксированное количество знаков после запятой:
        // например, значение 6.5 будет напечатано как 6.5, а не 6.500000.
        std::cout << "Миссия выполнима\n";
        std::cout << "Минимальное время: " << result.minimum_duration << '\n';
        std::cout << "Максимальная выгода: " << result.maximum_profit << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
