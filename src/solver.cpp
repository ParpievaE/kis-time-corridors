#include "solver.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// Состояние поиска описывает один из вариантов прохождения графа.
// Важно хранить не только текущую вершину, но и прошедшее время и собранные
// артефакты: от этого зависят дедлайны и доступность следующих коридоров.
struct State {
    int node{};
    std::int64_t elapsed{};
    std::string collected; // Один байт на предмет: '0' - не собран, '1' - собран.
};

// Создаёт уникальный ключ состояния для множества visited.
// Если такое же состояние уже обрабатывалось, повторно исследовать его
// не нужно: это помогает корректно обрабатывать циклы в графе.
std::string state_key(const State& state) {
    std::string key;
    key.reserve(sizeof(state.node) + sizeof(state.elapsed) + state.collected.size());
    key.append(reinterpret_cast<const char*>(&state.node), sizeof(state.node));
    key.append(reinterpret_cast<const char*>(&state.elapsed), sizeof(state.elapsed));
    key += state.collected;
    return key;
}

// Быстрая проверка: является ли текущая вершина одной из выходных.
// Массив exit_flags заранее подготовлен по списку выходов.
bool is_exit(int node, const std::vector<bool>& exit_flags) {
    return exit_flags[static_cast<std::size_t>(node)];
}

// Проверяет артефакты, расположенные в текущей вершине, и собирает те,
// которые ещё не были собраны и доступны в момент прибытия.
// Сбор происходит мгновенно; каждый артефакт можно получить только один раз.
void collect_at_node(
    int node,
    std::int64_t absolute_time,
    const std::vector<Artifact>& artifacts,
    const std::vector<std::vector<int>>& artifacts_at_node,
    std::string& collected
) {
    for (int artifact_index : artifacts_at_node[static_cast<std::size_t>(node)]) {
        const Artifact& artifact = artifacts[static_cast<std::size_t>(artifact_index)];
        if (collected[static_cast<std::size_t>(artifact_index)] != '\0') {
            continue;
        }

        const bool after_start = !artifact.window_start ||
                                 absolute_time >= *artifact.window_start;
        const bool before_end = !artifact.window_end ||
                                absolute_time <= *artifact.window_end;

        if (after_start && before_end) {
            collected[static_cast<std::size_t>(artifact_index)] = '\1';
        }
    }
}

// Складывает ценности всех уже собранных артефактов.
double collected_value(const std::string& collected,
                       const std::vector<Artifact>& artifacts) {
    double value = 0.0;
    for (std::size_t i = 0; i < artifacts.size(); ++i) {
        if (collected[i] != '\0') {
            value += artifacts[i].value;
        }
    }
    return value;
}

}

// Основная функция решения задачи. Она проверяет исходные данные,
// подготавливает граф и затем перебирает допустимые моменты начала миссии.
// Для каждого момента старта исследуются все достижимые состояния маршрута.
Result solve(const Problem& problem) {
    if (problem.time_limit < 0) {
        throw std::invalid_argument("T_lim не может быть отрицательным.");
    }
    if (!(problem.time_cost > 0.0) || !std::isfinite(problem.time_cost)) {
        throw std::invalid_argument("T_cost должен быть конечным числом больше нуля.");
    }
    if (problem.node_count <= 0 || problem.start_node < 0 ||
        problem.start_node >= problem.node_count) {
        throw std::invalid_argument("Некорректное количество вершин или начальная вершина.");
    }

    // Сначала проверяем артефакты и строим индекс: для каждой вершины
    // запоминаем, какие артефакты в ней можно подобрать. Это позволяет
    // не просматривать весь список артефактов при каждом посещении вершины.
    const std::size_t artifact_count = problem.artifacts.size();
    std::vector<std::vector<int>> artifacts_at_node(
        static_cast<std::size_t>(problem.node_count));
    for (std::size_t i = 0; i < artifact_count; ++i) {
        const Artifact& artifact = problem.artifacts[i];
        if (artifact.node < 0 || artifact.node >= problem.node_count) {
            throw std::invalid_argument("Предмет ссылается на несуществующую вершину.");
        }
        if (artifact.window_start && artifact.window_end &&
            *artifact.window_start > *artifact.window_end) {
            throw std::invalid_argument("У предмета указано некорректное временное окно.");
        }
        artifacts_at_node[static_cast<std::size_t>(artifact.node)].push_back(
            static_cast<int>(i));
    }

    // Строим список исходящих коридоров для каждой вершины.
    // Благодаря этому из текущей вершины рассматриваются только те переходы,
    // которые действительно начинаются в ней.
    std::vector<std::vector<int>> outgoing(
        static_cast<std::size_t>(problem.node_count));
    for (std::size_t i = 0; i < problem.corridors.size(); ++i) {
        const Corridor& edge = problem.corridors[i];
        if (edge.from < 0 || edge.from >= problem.node_count ||
            edge.to < 0 || edge.to >= problem.node_count) {
            throw std::invalid_argument("Коридор ссылается на несуществующую вершину.");
        }
        if (edge.duration < 0) {
            throw std::invalid_argument("Длительность коридора не может быть отрицательной.");
        }
        for (int required : edge.required_artifacts) {
            if (required < 0 || static_cast<std::size_t>(required) >= artifact_count) {
                throw std::invalid_argument("Коридор ссылается на несуществующий предмет.");
            }
        }
        outgoing[static_cast<std::size_t>(edge.from)].push_back(
            static_cast<int>(i));
    }

    // Превращаем список выходов в массив флагов: true означает,
    // что достижение данной вершины завершает миссию.
    std::vector<bool> exit_flags(static_cast<std::size_t>(problem.node_count), false);
    for (int exit_node : problem.exits) {
        if (exit_node < 0 || exit_node >= problem.node_count) {
            throw std::invalid_argument("Выход ссылается на несуществующую вершину.");
        }
        exit_flags[static_cast<std::size_t>(exit_node)] = true;
    }

    // В result накапливаются итоговые ответы. Пока ни одного допустимого
    // маршрута не найдено, feasible остаётся false.
    Result result;
    result.minimum_duration = 0;
    result.maximum_profit = 0.0;

    // В принятом формате временные значения целочисленные. Поэтому проверяем
    // каждый целочисленный момент начала миссии от -T_lim до T_lim включительно.
    // Отрицательное время старта допускается условием; его модуль учитывается
    // в общей длительности миссии. Времена за пределами этого диапазона не могут
    // дать длительность не больше T_lim.
    for (std::int64_t start_time = -problem.time_limit;; ++start_time) {
        const std::int64_t start_abs = (start_time < 0) ? -start_time : start_time;
        if (start_abs <= problem.time_limit) {
            // Очередь pending организует поиск в ширину по состояниям.
            // Множество visited не даёт бесконечно обходить один и тот же цикл
            // при одинаковой вершине, времени и наборе собранных артефактов.
            std::queue<State> pending;
            std::unordered_set<std::string> visited;

            // Начальное состояние: миссия ещё не прошла ни одного коридора,
            // а текущая вершина - стартовая. Проверяем, можно ли сразу собрать
            // артефакты в стартовой вершине в выбранный момент начала.
            State initial;
            initial.node = problem.start_node;
            initial.elapsed = 0;
            initial.collected.assign(artifact_count, '\0');
            collect_at_node(initial.node, start_time, problem.artifacts,
                            artifacts_at_node, initial.collected);

            pending.push(initial);
            visited.insert(state_key(initial));

            while (!pending.empty()) {
                State current = std::move(pending.front());
                pending.pop();

                // Полная длительность состоит из модуля времени начала и времени,
                // потраченного на прохождение коридоров. Если лимит уже превышен,
                // продолжать этот маршрут бессмысленно.
                const std::int64_t total_duration = start_abs + current.elapsed;
                if (total_duration > problem.time_limit) {
                    continue;
                }

                // При достижении любой выходной вершины миссия немедленно завершается:
                // продолжать маршрут после выхода нельзя.
                if (is_exit(current.node, exit_flags)) {
                    // Допущение: успешная миссия должна собрать
                    // хотя бы один артефакт. В формальном тексте это явно не указано,
                    // но такое решение принято для согласования с ожидаемым примером.
                    bool has_artifact = false;
                    for (char bit : current.collected) {
                        if (bit != '\0') {
                            has_artifact = true;
                            break;
                        }
                    }
                    if (!has_artifact) {
                        continue;
                    }

                    // Выгода равна сумме ценностей собранных артефактов минус
                    // стоимость потраченного времени: duration / T_cost.
                    // Этот маршрут может отличаться от маршрута с минимальным временем.
                    const double profit =
                        collected_value(current.collected, problem.artifacts) -
                        static_cast<double>(total_duration) / problem.time_cost;

                    if (!result.feasible || total_duration < result.minimum_duration) {
                        result.minimum_duration = total_duration;
                    }
                    if (!result.feasible || profit > result.maximum_profit) {
                        result.maximum_profit = profit;
                    }
                    result.feasible = true;
                    continue;
                }

                // Рассматриваем каждый коридор, выходящий из текущей вершины.
                // Перед переходом нужно проверить требования к артефактам,
                // лимит длительности миссии и дедлайн коридора.
                for (int edge_index : outgoing[static_cast<std::size_t>(current.node)]) {
                    const Corridor& edge =
                        problem.corridors[static_cast<std::size_t>(edge_index)];

                    // Все указанные для коридора артефакты должны быть собраны
                    // до входа в коридор. Требования соединяются логическим И.
                    bool requirements_met = true;
                    for (int required : edge.required_artifacts) {
                        if (current.collected[static_cast<std::size_t>(required)] == '\0') {
                            requirements_met = false;
                            break;
                        }
                    }
                    if (!requirements_met) {
                        continue;
                    }

                    if (edge.duration > problem.time_limit - total_duration) {
                        continue;
                    }

                    // Рассчитываем время после прохождения коридора и абсолютное
                    // время прибытия. Дедлайн проверяется именно в точке прибытия;
                    // прибытие точно в дедлайн разрешено.
                    const std::int64_t next_elapsed = current.elapsed + edge.duration;
                    const std::int64_t arrival_time = start_time + next_elapsed;
                    if (edge.deadline && arrival_time > *edge.deadline) {
                        continue;
                    }

                    // Формируем новое состояние в конечной вершине коридора.
                    // Копируем набор собранных артефактов, затем проверяем,
                    // не появились ли в новой вершине доступные для сбора предметы.
                    State next;
                    next.node = edge.to;
                    next.elapsed = next_elapsed;
                    next.collected = current.collected;
                    collect_at_node(next.node, arrival_time, problem.artifacts,
                                    artifacts_at_node, next.collected);

                    // Добавляем новое состояние в очередь только в том случае,
                    // если оно ещё не встречалось. Так поиск избегает повторной
                    // обработки идентичных состояний, в том числе в циклах.
                    std::string key = state_key(next);
                    if (visited.insert(std::move(key)).second) {
                        pending.push(std::move(next));
                    }
                }
            }
        }

        if (start_time == problem.time_limit) {
            break;
        }
    }

    // Возвращаем лучшие найденные показатели. Если допустимый маршрут,
    // удовлетворяющий принятому допущению о сборе артефакта, не найден,
    // поле feasible останется false.
    return result;
}
