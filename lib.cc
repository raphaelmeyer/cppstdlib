#include "lib.h"

#include <expected>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

struct ParsingError {
  size_t line;
  std::string message;
};

struct ValidationError {
  std::string message;
};

struct CycleDetectionError {
  std::string message;
};

using Error = std::variant<ParsingError, ValidationError, CycleDetectionError>;

template <typename T> using Result = std::expected<T, Error>;

std::string to_string(ParsingError const &error) {
  return std::format("parse error at line {}: {}", error.line, error.message);
}

std::string to_string(ValidationError const &error) {
  return std::format("validation error: {}", error.message);
}

std::string to_string(CycleDetectionError const &error) {
  return std::format("cycle error: {}", error.message);
}

void print_error(FILE *out, Error const &error) {
  std::visit([&out](auto const &e) { std::println(out, "{}", to_string(e)); },
             error);
}

struct Line {
  size_t number;
  std::string_view text;
};

std::string_view trim(std::string_view text) {
  auto const start = text.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos) {
    return {};
  }

  auto const end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

auto split_lines(std::string_view text) {
  return text | std::views::split('\n') | std::views::enumerate |
         std::views::transform([](auto &&tuple) {
           auto [index, line] = tuple;
           return Line{.number = static_cast<size_t>(index) + 1,
                       .text = trim(std::string_view{line})};
         }) |
         std::ranges::to<std::vector>();
}

auto split_words(std::string_view text) {
  return text | std::views::split(' ') | std::views::transform([](auto &&word) {
           return std::string_view{word};
         }) |
         std::views::filter([](auto const &word) { return not word.empty(); }) |
         std::ranges::to<std::vector>();
}

bool is_comment_or_empty(std::string_view line) {
  return line.empty() || line.at(0) == '#';
}

std::optional<int> parse_int(std::string_view text) {
  const auto first = text.data();
  const auto last = first + text.size();
  int value = 0;
  auto [end, error] = std::from_chars(first, last, value, 10);
  if (error != std::errc()) {
    return std::nullopt;
  }
  if (end != last) {
    return std::nullopt;
  }
  return value;
}

template <typename T> using Buffer = std::vector<T>;

using Name = std::string;
using Dependencies = std::vector<Name>;
using Features = std::vector<Name>;

struct Package {
  Name name{};
  std::optional<int> version{};
  std::optional<int> size{};
  Dependencies depends{};
  Features features{};
};

struct Task {
  Name name{};
  Name uses_package{};
  std::optional<int> cost{};
  std::vector<Name> requires_tasks{};
};

using Packages = std::vector<Package>;
using Tasks = std::vector<Task>;

struct Config {
  Packages packages{};
  Tasks tasks{};
};

std::optional<std::size_t> find_package_index(const Config &cfg,
                                              std::string_view name) {
  auto const package = std::ranges::find(cfg.packages, name, &Package::name);
  if (package == cfg.packages.end()) {
    return std::nullopt;
  }

  return std::distance(cfg.packages.begin(), package);
}

std::optional<std::size_t> find_task_index(const Config &cfg,
                                           std::string_view name) {
  auto const task = std::ranges::find(cfg.tasks, name, &Task::name);
  if (task == cfg.tasks.end()) {
    return std::nullopt;
  }

  return std::distance(cfg.tasks.begin(), task);
}

using ParseState = std::variant<std::monostate, Package, Task>;

void commit_previous_parse_state(Config &config, ParseState &state) {
  std::visit(
      overloaded{[&](Package &package) { config.packages.push_back(package); },
                 [&](Task &task) { config.tasks.push_back(task); },
                 [](auto) {}},
      state);
}

Result<Config> parse_config(std::string_view config_text) {
  auto const lines = split_lines(config_text);

  Config config{};
  ParseState state{};

  for (auto const &line : lines) {
    if (is_comment_or_empty(line.text)) {
      continue;
    }

    auto const words = split_words(line.text);
    if (words.empty()) {
      continue;
    }

    if (words.at(0) == "package") {
      commit_previous_parse_state(config, state);

      if (words.size() != 2) {
        return std::unexpected{
            ParsingError{line.number, "package requires exactly one name"}};
      }
      if (find_package_index(config, words.at(1)).has_value()) {
        return std::unexpected{ParsingError{line.number, "duplicate package"}};
      }

      state = Package{.name{words.at(1)}};
      continue;
    }

    if (words[0] == "task") {
      commit_previous_parse_state(config, state);

      if (words.size() != 2) {
        return std::unexpected{
            ParsingError{line.number, "task requires exactly one name"}};
      }
      if (find_task_index(config, words[1]).has_value()) {
        return std::unexpected{ParsingError{line.number, "duplicate task"}};
      };

      state = Task{.name{words.at(1)}};
      continue;
    }

    if (auto p = std::get_if<Package>(&state); p != nullptr) {
      if (words[0] == "version") {
        if (words.size() != 2) {
          return std::unexpected{
              ParsingError{line.number, "version requires one integer"}};
        }
        auto const version = parse_int(words[1]);
        if (not version.has_value()) {
          return std::unexpected{
              ParsingError{line.number, "invalid version integer"}};
        }
        p->version = version.value();
        continue;
      }

      if (words[0] == "size") {
        if (words.size() != 2) {
          return std::unexpected{
              ParsingError{line.number, "size requires one integer"}};
        }
        auto const size = parse_int(words[1]);
        if (not size.has_value()) {
          return std::unexpected{
              ParsingError{line.number, "invalid size integer"}};
        }
        p->size = size.value();
        continue;
      }

      if (words[0] == "depends") {
        if (words.size() < 2) {
          return std::unexpected{ParsingError{
              line.number, "depends requires at least one package name"}};
        }
        for (std::size_t i = 1; i < words.size(); ++i) {
          p->depends.emplace_back(words[i]);
        }
        continue;
      }

      if (words[0] == "feature") {
        if (words.size() != 2) {
          return std::unexpected{
              ParsingError{line.number, "feature requires one name"}};
        }
        p->features.emplace_back(words[1]);
        continue;
      }

      return std::unexpected{
          ParsingError{line.number, "unknown package directive"}};
    }

    if (auto t = std::get_if<Task>(&state); t != nullptr) {
      if (words[0] == "uses") {
        if (words.size() != 2) {
          return std::unexpected{
              ParsingError{line.number, "uses requires one package name"}};
        }
        t->uses_package = words[1];
        continue;
      }

      if (words[0] == "cost") {
        if (words.size() != 2) {
          return std::unexpected{
              ParsingError{line.number, "cost requires one integer"}};
        }
        auto const cost = parse_int(words[1]);
        if (not cost.has_value()) {
          return std::unexpected{
              ParsingError{line.number, "invalid cost integer"}};
        }
        t->cost = cost.value();
        continue;
      }

      if (words[0] == "requires") {
        if (words.size() < 2) {
          return std::unexpected{ParsingError{
              line.number, "requires needs at least one task name"}};
        }
        for (std::size_t i = 1; i < words.size(); ++i) {
          t->requires_tasks.emplace_back(words[i]);
        }
        continue;
      }

      return std::unexpected{
          ParsingError{line.number, "unknown task directive"}};
    }

    return std::unexpected{ParsingError{
        line.number, "directive outside of package or task block"}};
  }

  commit_previous_parse_state(config, state);
  return config;
}

Result<void> validate_config(const Config &cfg) {
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    const Package &p = cfg.packages[i];
    if (not p.version.has_value()) {
      return std::unexpected{ValidationError{"package missing version"}};
    }
    if (not p.size.has_value()) {
      return std::unexpected{ValidationError{"package missing size"}};
    }

    for (std::size_t d = 0; d < p.depends.size(); ++d) {
      if (not find_package_index(cfg, p.depends[d]).has_value()) {
        return std::unexpected{
            ValidationError{"package dependency refers to unknown package"}};
      }
    }
  }

  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    const Task &t = cfg.tasks[i];
    if (t.uses_package.empty()) {
      return std::unexpected{ValidationError{"task missing uses"}};
    }
    if (not t.cost.has_value()) {
      return std::unexpected{ValidationError{"task missing cost"}};
    }
    if (not find_package_index(cfg, t.uses_package).has_value()) {
      return std::unexpected{ValidationError{"task uses unknown package"}};
    }
    for (std::size_t r = 0; r < t.requires_tasks.size(); ++r) {
      if (not find_task_index(cfg, t.requires_tasks[r]).has_value()) {
        return std::unexpected{ValidationError{"task requires unknown task"}};
      }
    }
  }

  return {};
}

Result<void> detect_package_cycle_dfs(const Config &cfg, int index,
                                      Buffer<int> &color) {
  color.at(index) = 1;
  const Package &p = cfg.packages[index];

  for (std::size_t i = 0; i < p.depends.size(); ++i) {
    const auto dependency = find_package_index(cfg, p.depends[i]);
    if (not dependency.has_value()) {
      continue;
    }

    if (color.at(dependency.value()) == 1) {
      return std::unexpected{CycleDetectionError{"package cycle detected"}};
    }
    if (color.at(dependency.value()) == 0) {
      Result r = detect_package_cycle_dfs(cfg, dependency.value(), color);
      if (not r.has_value()) {
        return r;
      }
    }
  }

  color.at(index) = 2;
  return {};
}

Result<void> detect_task_cycle_dfs(const Config &cfg, int index,
                                   Buffer<int> &color) {
  color.at(index) = 1;
  const Task &t = cfg.tasks[index];

  for (std::size_t i = 0; i < t.requires_tasks.size(); ++i) {
    auto const dependency = find_task_index(cfg, t.requires_tasks[i]);
    if (not dependency.has_value()) {
      continue;
    }
    if (color.at(dependency.value()) == 1) {
      return std::unexpected{CycleDetectionError{"task cycle detected"}};
    }
    if (color.at(dependency.value()) == 0) {
      Result r = detect_task_cycle_dfs(cfg, dependency.value(), color);
      if (not r.has_value()) {
        return r;
      }
    }
  }

  color.at(index) = 2;
  return {};
}

Result<void> detect_cycles(const Config &cfg) {
  Buffer<int> pcolor;
  pcolor.reserve(cfg.packages.size());
  for (std::size_t i = 0; i < cfg.packages.size(); ++i)
    pcolor.push_back(0);

  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    if (pcolor[i] == 0) {
      Result r = detect_package_cycle_dfs(cfg, i, pcolor);
      if (not r.has_value()) {
        return r;
      }
    }
  }

  Buffer<int> tcolor;
  tcolor.reserve(cfg.tasks.size());
  for (std::size_t i = 0; i < cfg.tasks.size(); ++i)
    tcolor.push_back(0);

  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    if (tcolor[i] == 0) {
      Result r = detect_task_cycle_dfs(cfg, i, tcolor);
      if (not r.has_value()) {
        return r;
      }
    }
  }

  return {};
}

void topo_packages_dfs(const Config &cfg, int index, Buffer<int> &visited,
                       Buffer<int> &out_order) {
  visited.at(index) = 1;
  const Package &p = cfg.packages[index];
  for (std::size_t i = 0; i < p.depends.size(); ++i) {
    auto const dependency = find_package_index(cfg, p.depends[i]);
    if (dependency.has_value() and not visited.at(dependency.value())) {
      topo_packages_dfs(cfg, dependency.value(), visited, out_order);
    }
  }
  out_order.push_back(index);
}

Buffer<int> package_build_order(const Config &cfg) {
  Buffer<int> visited;
  visited.reserve(cfg.packages.size());
  for (std::size_t i = 0; i < cfg.packages.size(); ++i)
    visited.push_back(0);

  Buffer<int> order;
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    if (!visited[i]) {
      topo_packages_dfs(cfg, i, visited, order);
    }
  }
  return order;
}

int compute_transitive_size_dfs(const Config &cfg, int index, Buffer<int> &memo,
                                Buffer<int> &busy) {
  if (memo[index] >= 0)
    return memo[index];
  if (busy[index])
    return 0;

  busy[index] = 1;
  auto total = cfg.packages[index].size.value();
  for (std::size_t i = 0; i < cfg.packages[index].depends.size(); ++i) {
    auto const dependency =
        find_package_index(cfg, cfg.packages[index].depends[i]);
    if (dependency.has_value()) {
      total += compute_transitive_size_dfs(cfg, dependency.value(), memo, busy);
    }
  }
  busy[index] = 0;
  memo[index] = total;
  return total;
}

struct PackageReport {
  Name name;
  int version;
  int direct_size;
  int transitive_size;
  int dependency_count;
  int feature_count;
};

Buffer<PackageReport> make_package_reports(const Config &cfg) {
  Buffer<int> memo;
  memo.reserve(cfg.packages.size());
  for (std::size_t i = 0; i < cfg.packages.size(); ++i)
    memo.push_back(-1);

  Buffer<int> busy;
  busy.reserve(cfg.packages.size());
  for (std::size_t i = 0; i < cfg.packages.size(); ++i)
    busy.push_back(0);

  Buffer<PackageReport> reports;
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    PackageReport r;
    r.name = cfg.packages[i].name;
    r.version = cfg.packages[i].version.value();
    r.direct_size = cfg.packages[i].size.value();
    r.transitive_size = compute_transitive_size_dfs(cfg, i, memo, busy);
    r.dependency_count = cfg.packages[i].depends.size();
    r.feature_count = cfg.packages[i].features.size();
    reports.push_back(r);
  }

  return reports;
}

struct TaskReport {
  Name name;
  Name package_name;
  int cost;
  int prerequisite_count;
  int package_transitive_size;
};

int transitive_size_for_package(const Config &cfg, int pkg_index) {
  Buffer<int> memo;
  memo.reserve(cfg.packages.size());
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    memo.push_back(-1);
  }
  Buffer<int> busy;
  busy.reserve(cfg.packages.size());
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    busy.push_back(0);
  }
  return compute_transitive_size_dfs(cfg, pkg_index, memo, busy);
}

Buffer<TaskReport> make_task_reports(const Config &cfg) {
  Buffer<TaskReport> reports;
  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    TaskReport r;
    r.name = cfg.tasks[i].name;
    r.package_name = cfg.tasks[i].uses_package;
    r.cost = cfg.tasks[i].cost.value();
    r.prerequisite_count = cfg.tasks[i].requires_tasks.size();
    auto const package = find_package_index(cfg, cfg.tasks[i].uses_package);
    r.package_transitive_size =
        package.has_value() ? transitive_size_for_package(cfg, package.value())
                            : 0;
    reports.push_back(r);
  }
  return reports;
}

void sort_package_reports_by_transitive_size(Buffer<PackageReport> &reports) {
  for (std::size_t i = 0; i < reports.size(); ++i) {
    for (std::size_t j = i + 1; j < reports.size(); ++j) {
      if (reports[j].transitive_size > reports[i].transitive_size) {
        PackageReport tmp = reports[i];
        reports[i] = reports[j];
        reports[j] = tmp;
      }
    }
  }
}

void sort_task_reports_by_cost(Buffer<TaskReport> &reports) {
  for (std::size_t i = 0; i < reports.size(); ++i) {
    for (std::size_t j = i + 1; j < reports.size(); ++j) {
      if (reports[j].cost > reports[i].cost) {
        TaskReport tmp = reports[i];
        reports[i] = reports[j];
        reports[j] = tmp;
      }
    }
  }
}

int total_package_direct_size(const Config &cfg) {
  std::size_t total = 0;
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    total += cfg.packages[i].size.value();
  }
  return total;
}

int total_task_cost(const Config &cfg) {
  int total = 0;
  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    total += cfg.tasks[i].cost.value();
  }
  return total;
}

int find_heaviest_package_index(const Buffer<PackageReport> &reports) {
  if (reports.empty())
    return -1;

  int best = 0;
  for (std::size_t i = 1; i < reports.size(); ++i) {
    if (reports[i].transitive_size > reports[best].transitive_size) {
      best = i;
    }
  }

  return best;
}

enum QueryKind { QUERY_NONE, QUERY_PACKAGE, QUERY_TASK };

struct QueryResult {
  QueryKind kind;
  int index;
};

QueryResult query_entity_by_name(const Config &cfg, std::string_view name) {
  QueryResult q;
  q.kind = QUERY_NONE;
  q.index = -1;

  auto const package = find_package_index(cfg, name);
  if (package.has_value()) {
    q.kind = QUERY_PACKAGE;
    q.index = package.value();
    return q;
  }

  auto const task = find_task_index(cfg, name);
  if (task.has_value()) {
    q.kind = QUERY_TASK;
    q.index = task.value();
    return q;
  }

  return q;
}

void print_line(FILE *out) {
  std::println(out,
               "------------------------------------------------------------");
}

void print_package(FILE *out, const Package &p) {
  std::println(out, "package {}", p.name);
  std::println(out, "  version: {}", p.version.value());
  std::println(out, "  size: {}", p.size.value());

  std::print(out, "  depends:");
  if (p.depends.empty()) {
    std::print(out, " <none>");
  } else {
    for (std::size_t i = 0; i < p.depends.size(); ++i) {
      std::print(out, " {}", p.depends[i]);
    }
  }
  std::println(out);

  std::print(out, "  features:");
  if (p.features.empty()) {
    std::print(out, " <none>");
  } else {
    for (std::size_t i = 0; i < p.features.size(); ++i) {
      std::print(out, " {}", p.features[i]);
    }
  }
  std::println(out);
}

void print_task(FILE *out, const Task &t) {
  std::println(out, "task {}", t.name);
  std::println(out, "  uses: {}", t.uses_package);
  std::println(out, "  cost: {}", t.cost.value());

  std::print(out, "  requires:");
  if (t.requires_tasks.empty()) {
    std::print(out, " <none>");
  } else {
    for (std::size_t i = 0; i < t.requires_tasks.size(); ++i) {
      std::print(out, " {}", t.requires_tasks[i]);
    }
  }
  std::println(out);
}

void print_build_order(FILE *out, const Config &cfg, const Buffer<int> &order) {
  std::println(out, "package build order:");
  for (std::size_t i = 0; i < order.size(); ++i) {
    std::println(out, "  {}. {}", i + 1, cfg.packages[order[i]].name);
  }
}

void print_package_reports(FILE *out, const Buffer<PackageReport> &reports) {
  std::println(out, "package reports (by transitive size desc):");
  for (std::size_t i = 0; i < reports.size(); ++i) {
    const PackageReport &r = reports[i];
    std::println(
        out, "  {:<10} version={} direct={} transitive={} deps={} features={}",
        r.name, r.version, r.direct_size, r.transitive_size, r.dependency_count,
        r.feature_count);
  }
}

void print_task_reports(FILE *out, const Buffer<TaskReport> &reports) {
  std::println(out, "task reports (by cost desc):");
  for (std::size_t i = 0; i < reports.size(); ++i) {
    const TaskReport &r = reports[i];
    std::println(out,
                 "  {:<10} uses={:<10} cost={} prereqs={} pkg-transitive={}",
                 r.name, r.package_name, r.cost, r.prerequisite_count,
                 r.package_transitive_size);
  }
}

void print_summary(FILE *out, const Config &cfg,
                   const Buffer<PackageReport> &package_reports) {
  std::println(out, "summary:");
  std::println(out, "  package count: {}", cfg.packages.size());
  std::println(out, "  task count: {}", cfg.tasks.size());
  std::println(out, "  total package direct size: {}",
               total_package_direct_size(cfg));
  std::println(out, "  total task cost: {}", total_task_cost(cfg));

  int heavy = find_heaviest_package_index(package_reports);
  if (heavy >= 0) {
    const PackageReport &r = package_reports[heavy];
    std::println(out, "  heaviest package: {} ({})", r.name, r.transitive_size);
  } else {
    std::println(out, "  heaviest package: <none>");
  }
}

void print_query(FILE *out, const Config &cfg, std::string_view name) {
  QueryResult q = query_entity_by_name(cfg, name);
  if (q.kind == QUERY_NONE) {
    std::println(out, "query '{}': not found", name);
    return;
  }

  if (q.kind == QUERY_PACKAGE) {
    std::println(out, "query '{}': found package", name);
    print_package(out, cfg.packages[q.index]);
    return;
  }

  if (q.kind == QUERY_TASK) {
    std::println(out, "query '{}': found task", name);
    print_task(out, cfg.tasks[q.index]);
    return;
  }
}

} // namespace

int run(std::string_view config_text, FILE *out) {
  auto const result =
      parse_config(config_text).and_then([](auto const &config) {
        return validate_config(config)
            .and_then([&config] { return detect_cycles(config); })
            .and_then([&config] -> Result<Config> { return config; });
      });

  if (not result.has_value()) {
    print_error(out, result.error());
    return 1;
  }

  auto const cfg = result.value();

  print_line(out);
  std::println(out, "raw config objects");
  print_line(out);

  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    print_package(out, cfg.packages[i]);
  }
  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    print_task(out, cfg.tasks[i]);
  }

  print_line(out);
  Buffer<int> order = package_build_order(cfg);
  print_build_order(out, cfg, order);

  print_line(out);
  Buffer<PackageReport> package_reports = make_package_reports(cfg);
  sort_package_reports_by_transitive_size(package_reports);
  print_package_reports(out, package_reports);

  print_line(out);
  Buffer<TaskReport> task_reports = make_task_reports(cfg);
  sort_task_reports_by_cost(task_reports);
  print_task_reports(out, task_reports);

  print_line(out);
  print_summary(out, cfg, package_reports);

  print_line(out);
  print_query(out, cfg, "app");
  print_line(out);
  print_query(out, cfg, "package");
  print_line(out);
  print_query(out, cfg, "missing");
  print_line(out);

  return 0;
}
