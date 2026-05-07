#include "lib.h"

#include <expected>
#include <string>
#include <vector>

static const int MAX_NAME = 64;
static const int MAX_LINE = 256;

template <typename T> static T &&rvalue_reference(T &v) {
  return static_cast<T &&>(v);
}

static int cstr_len(const char *s) {
  int n = 0;
  while (s[n] != '\0') {
    ++n;
  }
  return n;
}

static bool cstr_eq(const char *a, const char *b) {
  int i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i])
      return false;
    ++i;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static void cstr_copy(char *dst, const char *src, int cap) {
  if (cap <= 0)
    return;
  int i = 0;
  while (src[i] != '\0' && i < cap - 1) {
    dst[i] = src[i];
    ++i;
  }
  dst[i] = '\0';
}

static bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void trim_in_place(char *s) {
  int n = cstr_len(s);
  int start = 0;
  while (start < n && is_space(s[start]))
    ++start;
  int end = n;
  while (end > start && is_space(s[end - 1]))
    --end;

  int out = 0;
  for (int i = start; i < end; ++i) {
    s[out++] = s[i];
  }
  s[out] = '\0';
}

static bool parse_int(const char *s, int &out) {
  int i = 0;
  bool neg = false;
  if (s[i] == '-') {
    neg = true;
    ++i;
  }
  if (s[i] == '\0')
    return false;
  int v = 0;
  while (s[i] != '\0') {
    char c = s[i];
    if (c < '0' || c > '9')
      return false;
    v = v * 10 + (c - '0');
    ++i;
  }
  out = neg ? -v : v;
  return true;
}

template <typename T> using Buffer = std::vector<T>;

struct Error {
  int line;
  std::string message;
};

template <typename T> using Result = std::expected<T, Error>;

struct Name {
  char text[MAX_NAME];
};

static Name make_name(const char *s) {
  Name n;
  cstr_copy(n.text, s, MAX_NAME);
  return n;
}

static bool name_eq(const Name &a, const Name &b) {
  return cstr_eq(a.text, b.text);
}

struct Package {
  Name name;
  int version;
  int size;
  Buffer<Name> depends;
  Buffer<Name> features;
  bool has_version;
  bool has_size;
};

struct Task {
  Name name;
  Name uses_package;
  int cost;
  Buffer<Name> requires_tasks;
  bool has_uses;
  bool has_cost;
};

struct Config {
  Buffer<Package> packages;
  Buffer<Task> tasks;
};

static Package make_empty_package(const char *name) {
  Package p;
  p.name = make_name(name);
  p.version = 0;
  p.size = 0;
  p.has_version = false;
  p.has_size = false;
  return p;
}

static Task make_empty_task(const char *name) {
  Task t;
  t.name = make_name(name);
  t.uses_package = make_name("");
  t.cost = 0;
  t.has_uses = false;
  t.has_cost = false;
  return t;
}

static int find_package_index(const Config &cfg, const Name &n) {
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    if (name_eq(cfg.packages[i].name, n))
      return i;
  }
  return -1;
}

static int find_task_index(const Config &cfg, const Name &n) {
  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    if (name_eq(cfg.tasks[i].name, n))
      return i;
  }
  return -1;
}

static bool split_words(const char *line, Buffer<Name> &out_words) {
  out_words.clear();
  char temp[MAX_LINE];
  cstr_copy(temp, line, MAX_LINE);

  int i = 0;
  while (temp[i] != '\0') {
    while (temp[i] != '\0' && is_space(temp[i]))
      ++i;
    if (temp[i] == '\0')
      break;

    char word[MAX_NAME];
    int w = 0;
    while (temp[i] != '\0' && !is_space(temp[i])) {
      if (w < MAX_NAME - 1) {
        word[w++] = temp[i];
      }
      ++i;
    }
    word[w] = '\0';
    out_words.push_back(make_name(word));
  }
  return true;
}

struct LineReader {
  const char *text;
  int pos;
  int line;
};

static void line_reader_init(LineReader &r, const char *text) {
  r.text = text;
  r.pos = 0;
  r.line = 1;
}

struct ReadLineResult {
  bool has_line;
  char line[MAX_LINE];
  int line_no;
};

static ReadLineResult line_reader_next(LineReader &r) {
  ReadLineResult res;
  res.has_line = false;
  res.line[0] = '\0';
  res.line_no = r.line;

  if (r.text[r.pos] == '\0') {
    return res;
  }

  int out = 0;
  while (r.text[r.pos] != '\0' && r.text[r.pos] != '\n') {
    if (out < MAX_LINE - 1) {
      res.line[out++] = r.text[r.pos];
    }
    ++r.pos;
  }
  if (r.text[r.pos] == '\n') {
    ++r.pos;
    ++r.line;
  }
  res.line[out] = '\0';
  res.has_line = true;
  return res;
}

static bool is_comment_or_empty(const char *line) {
  char tmp[MAX_LINE];
  cstr_copy(tmp, line, MAX_LINE);
  trim_in_place(tmp);
  return tmp[0] == '\0' || tmp[0] == '#';
}

static Result<Config> parse_config(const char *text) {
  LineReader reader;
  line_reader_init(reader, text);

  Config config{};

  enum ParseState { STATE_NONE, STATE_PACKAGE, STATE_TASK };

  ParseState state = STATE_NONE;
  int current_package = -1;
  int current_task = -1;

  while (true) {
    ReadLineResult lr = line_reader_next(reader);
    if (!lr.has_line)
      break;

    char line[MAX_LINE];
    cstr_copy(line, lr.line, MAX_LINE);
    trim_in_place(line);

    if (is_comment_or_empty(line)) {
      continue;
    }

    Buffer<Name> words;
    split_words(line, words);
    if (words.empty())
      continue;

    if (cstr_eq(words[0].text, "package")) {
      if (words.size() != 2) {
        return std::unexpected{
            Error{lr.line_no, "package requires exactly one name"}};
      }
      if (find_package_index(config, words[1]) >= 0) {
        return std::unexpected{Error{lr.line_no, "duplicate package"}};
      }
      config.packages.push_back(make_empty_package(words[1].text));
      current_package = config.packages.size() - 1;
      current_task = -1;
      state = STATE_PACKAGE;
      continue;
    }

    if (cstr_eq(words[0].text, "task")) {
      if (words.size() != 2) {
        return std::unexpected{
            Error{lr.line_no, "task requires exactly one name"}};
      }
      if (find_task_index(config, words[1]) >= 0) {
        return std::unexpected{Error{lr.line_no, "duplicate task"}};
      }
      Task t = make_empty_task(words[1].text);
      config.tasks.push_back(rvalue_reference(t));
      current_task = config.tasks.size() - 1;
      current_package = -1;
      state = STATE_TASK;
      continue;
    }

    if (state == STATE_PACKAGE) {
      Package &p = config.packages[current_package];

      if (cstr_eq(words[0].text, "version")) {
        if (words.size() != 2) {
          return std::unexpected{
              Error{lr.line_no, "version requires one integer"}};
        }
        int v = 0;
        if (!parse_int(words[1].text, v)) {
          return std::unexpected{Error{lr.line_no, "invalid version integer"}};
        }
        p.version = v;
        p.has_version = true;
        continue;
      }

      if (cstr_eq(words[0].text, "size")) {
        if (words.size() != 2) {
          return std::unexpected{
              Error{lr.line_no, "size requires one integer"}};
        }
        int v = 0;
        if (!parse_int(words[1].text, v)) {
          return std::unexpected{Error{lr.line_no, "invalid size integer"}};
        }
        p.size = v;
        p.has_size = true;
        continue;
      }

      if (cstr_eq(words[0].text, "depends")) {
        if (words.size() < 2) {
          return std::unexpected{
              Error{lr.line_no, "depends requires at least one package name"}};
        }
        for (std::size_t i = 1; i < words.size(); ++i) {
          p.depends.push_back(words[i]);
        }
        continue;
      }

      if (cstr_eq(words[0].text, "feature")) {
        if (words.size() != 2) {
          return std::unexpected{
              Error{lr.line_no, "feature requires one name"}};
        }
        p.features.push_back(words[1]);
        continue;
      }

      return std::unexpected{Error{lr.line_no, "unknown package directive"}};
    }

    if (state == STATE_TASK) {
      Task &t = config.tasks[current_task];

      if (cstr_eq(words[0].text, "uses")) {
        if (words.size() != 2) {
          return std::unexpected{
              Error{lr.line_no, "uses requires one package name"}};
        }
        t.uses_package = words[1];
        t.has_uses = true;
        continue;
      }

      if (cstr_eq(words[0].text, "cost")) {
        if (words.size() != 2) {
          return std::unexpected{
              Error{lr.line_no, "cost requires one integer"}};
        }
        int v = 0;
        if (!parse_int(words[1].text, v)) {
          return std::unexpected{Error{lr.line_no, "invalid cost integer"}};
        }
        t.cost = v;
        t.has_cost = true;
        continue;
      }

      if (cstr_eq(words[0].text, "requires")) {
        if (words.size() < 2) {
          return std::unexpected{
              Error{lr.line_no, "requires needs at least one task name"}};
        }
        for (std::size_t i = 1; i < words.size(); ++i) {
          t.requires_tasks.push_back(words[i]);
        }
        continue;
      }

      return std::unexpected{Error{lr.line_no, "unknown task directive"}};
    }

    return std::unexpected{
        Error{lr.line_no, "directive outside of package or task block"}};
  }

  return config;
}

static Result<void> validate_config(const Config &cfg) {
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    const Package &p = cfg.packages[i];
    if (!p.has_version) {
      return std::unexpected{Error{0, "package missing version"}};
    }
    if (!p.has_size) {
      return std::unexpected{Error{0, "package missing size"}};
    }

    for (std::size_t d = 0; d < p.depends.size(); ++d) {
      if (find_package_index(cfg, p.depends[d]) < 0) {
        return std::unexpected{
            Error{0, "package dependency refers to unknown package"}};
      }
    }
  }

  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    const Task &t = cfg.tasks[i];
    if (!t.has_uses) {
      return std::unexpected{Error{0, "task missing uses"}};
    }
    if (!t.has_cost) {
      return std::unexpected{Error{0, "task missing cost"}};
    }
    if (find_package_index(cfg, t.uses_package) < 0) {
      return std::unexpected{Error{0, "task uses unknown package"}};
    }
    for (std::size_t r = 0; r < t.requires_tasks.size(); ++r) {
      if (find_task_index(cfg, t.requires_tasks[r]) < 0) {
        return std::unexpected{Error{0, "task requires unknown task"}};
      }
    }
  }

  return {};
}

static Result<void> detect_package_cycle_dfs(const Config &cfg, int index,
                                             Buffer<int> &color) {
  color[index] = 1;
  const Package &p = cfg.packages[index];

  for (std::size_t i = 0; i < p.depends.size(); ++i) {
    int dep = find_package_index(cfg, p.depends[i]);
    if (dep < 0)
      continue;
    if (color[dep] == 1) {
      return std::unexpected{Error{0, "package cycle detected"}};
    }
    if (color[dep] == 0) {
      Result r = detect_package_cycle_dfs(cfg, dep, color);
      if (not r.has_value()) {
        return r;
      }
    }
  }

  color[index] = 2;
  return {};
}

static Result<void> detect_task_cycle_dfs(const Config &cfg, int index,
                                          Buffer<int> &color) {
  color[index] = 1;
  const Task &t = cfg.tasks[index];

  for (std::size_t i = 0; i < t.requires_tasks.size(); ++i) {
    int dep = find_task_index(cfg, t.requires_tasks[i]);
    if (dep < 0)
      continue;
    if (color[dep] == 1) {
      return std::unexpected{Error{0, "task cycle detected"}};
    }
    if (color[dep] == 0) {
      Result r = detect_task_cycle_dfs(cfg, dep, color);
      if (not r.has_value()) {
        return r;
      }
    }
  }

  color[index] = 2;
  return {};
}

static Result<void> detect_cycles(const Config &cfg) {
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

static void topo_packages_dfs(const Config &cfg, int index,
                              Buffer<int> &visited, Buffer<int> &out_order) {
  visited[index] = 1;
  const Package &p = cfg.packages[index];
  for (std::size_t i = 0; i < p.depends.size(); ++i) {
    int dep = find_package_index(cfg, p.depends[i]);
    if (dep >= 0 && !visited[dep]) {
      topo_packages_dfs(cfg, dep, visited, out_order);
    }
  }
  out_order.push_back(index);
}

static Buffer<int> package_build_order(const Config &cfg) {
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

static int compute_transitive_size_dfs(const Config &cfg, int index,
                                       Buffer<int> &memo, Buffer<int> &busy) {
  if (memo[index] >= 0)
    return memo[index];
  if (busy[index])
    return 0;

  busy[index] = 1;
  int total = cfg.packages[index].size;
  for (std::size_t i = 0; i < cfg.packages[index].depends.size(); ++i) {
    int dep = find_package_index(cfg, cfg.packages[index].depends[i]);
    if (dep >= 0) {
      total += compute_transitive_size_dfs(cfg, dep, memo, busy);
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

static Buffer<PackageReport> make_package_reports(const Config &cfg) {
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
    r.version = cfg.packages[i].version;
    r.direct_size = cfg.packages[i].size;
    r.transitive_size = compute_transitive_size_dfs(cfg, i, memo, busy);
    r.dependency_count = cfg.packages[i].depends.size();
    r.feature_count = cfg.packages[i].features.size();
    reports.push_back(rvalue_reference(r));
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

static int transitive_size_for_package(const Config &cfg, int pkg_index) {
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

static Buffer<TaskReport> make_task_reports(const Config &cfg) {
  Buffer<TaskReport> reports;
  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    TaskReport r;
    r.name = cfg.tasks[i].name;
    r.package_name = cfg.tasks[i].uses_package;
    r.cost = cfg.tasks[i].cost;
    r.prerequisite_count = cfg.tasks[i].requires_tasks.size();
    int p = find_package_index(cfg, cfg.tasks[i].uses_package);
    r.package_transitive_size =
        p >= 0 ? transitive_size_for_package(cfg, p) : 0;
    reports.push_back(rvalue_reference(r));
  }
  return reports;
}

static void
sort_package_reports_by_transitive_size(Buffer<PackageReport> &reports) {
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

static void sort_task_reports_by_cost(Buffer<TaskReport> &reports) {
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

static int total_package_direct_size(const Config &cfg) {
  std::size_t total = 0;
  for (std::size_t i = 0; i < cfg.packages.size(); ++i) {
    total += cfg.packages[i].size;
  }
  return total;
}

static int total_task_cost(const Config &cfg) {
  int total = 0;
  for (std::size_t i = 0; i < cfg.tasks.size(); ++i) {
    total += cfg.tasks[i].cost;
  }
  return total;
}

static int find_heaviest_package_index(const Buffer<PackageReport> &reports) {
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

static QueryResult query_entity_by_name(const Config &cfg, const char *name) {
  QueryResult q;
  q.kind = QUERY_NONE;
  q.index = -1;

  Name n = make_name(name);
  int p = find_package_index(cfg, n);
  if (p >= 0) {
    q.kind = QUERY_PACKAGE;
    q.index = p;
    return q;
  }

  int t = find_task_index(cfg, n);
  if (t >= 0) {
    q.kind = QUERY_TASK;
    q.index = t;
    return q;
  }

  return q;
}

static void print_line(FILE *out) {
  std::fprintf(
      out, "------------------------------------------------------------\n");
}

static void print_package(FILE *out, const Package &p) {
  std::fprintf(out, "package %s\n", p.name.text);
  std::fprintf(out, "  version: %d\n", p.version);
  std::fprintf(out, "  size: %d\n", p.size);

  std::fprintf(out, "  depends:");
  if (p.depends.empty()) {
    std::fprintf(out, " <none>");
  } else {
    for (std::size_t i = 0; i < p.depends.size(); ++i) {
      std::fprintf(out, " %s", p.depends[i].text);
    }
  }
  std::fprintf(out, "\n");

  std::fprintf(out, "  features:");
  if (p.features.empty()) {
    std::fprintf(out, " <none>");
  } else {
    for (std::size_t i = 0; i < p.features.size(); ++i) {
      std::fprintf(out, " %s", p.features[i].text);
    }
  }
  std::fprintf(out, "\n");
}

static void print_task(FILE *out, const Task &t) {
  std::fprintf(out, "task %s\n", t.name.text);
  std::fprintf(out, "  uses: %s\n", t.uses_package.text);
  std::fprintf(out, "  cost: %d\n", t.cost);

  std::fprintf(out, "  requires:");
  if (t.requires_tasks.empty()) {
    std::fprintf(out, " <none>");
  } else {
    for (std::size_t i = 0; i < t.requires_tasks.size(); ++i) {
      std::fprintf(out, " %s", t.requires_tasks[i].text);
    }
  }
  std::fprintf(out, "\n");
}

static void print_build_order(FILE *out, const Config &cfg,
                              const Buffer<int> &order) {
  std::fprintf(out, "package build order:\n");
  for (std::size_t i = 0; i < order.size(); ++i) {
    std::fprintf(out, "  %lu. %s\n", i + 1, cfg.packages[order[i]].name.text);
  }
}

static void print_package_reports(FILE *out,
                                  const Buffer<PackageReport> &reports) {
  std::fprintf(out, "package reports (by "
                    "transitive size desc):\n");
  for (std::size_t i = 0; i < reports.size(); ++i) {
    const PackageReport &r = reports[i];
    std::fprintf(out,
                 "  %-10s version=%d direct=%d "
                 "transitive=%d deps=%d features=%d\n",
                 r.name.text, r.version, r.direct_size, r.transitive_size,
                 r.dependency_count, r.feature_count);
  }
}

static void print_task_reports(FILE *out, const Buffer<TaskReport> &reports) {
  std::fprintf(out, "task reports (by cost desc):\n");
  for (std::size_t i = 0; i < reports.size(); ++i) {
    const TaskReport &r = reports[i];
    std::fprintf(out,
                 "  %-10s uses=%-10s cost=%d "
                 "prereqs=%d pkg-transitive=%d\n",
                 r.name.text, r.package_name.text, r.cost, r.prerequisite_count,
                 r.package_transitive_size);
  }
}

static void print_summary(FILE *out, const Config &cfg,
                          const Buffer<PackageReport> &package_reports) {
  std::fprintf(out, "summary:\n");
  std::fprintf(out, "  package count: %lu\n", cfg.packages.size());
  std::fprintf(out, "  task count: %lu\n", cfg.tasks.size());
  std::fprintf(out, "  total package direct size: %d\n",
               total_package_direct_size(cfg));
  std::fprintf(out, "  total task cost: %d\n", total_task_cost(cfg));

  int heavy = find_heaviest_package_index(package_reports);
  if (heavy >= 0) {
    const PackageReport &r = package_reports[heavy];
    std::fprintf(out, "  heaviest package: %s (%d)\n", r.name.text,
                 r.transitive_size);
  } else {
    std::fprintf(out, "  heaviest package: <none>\n");
  }
}

static void print_query(FILE *out, const Config &cfg, const char *name) {
  QueryResult q = query_entity_by_name(cfg, name);
  if (q.kind == QUERY_NONE) {
    std::fprintf(out, "query '%s': not found\n", name);
    return;
  }

  if (q.kind == QUERY_PACKAGE) {
    std::fprintf(out, "query '%s': found package\n", name);
    print_package(out, cfg.packages[q.index]);
    return;
  }

  if (q.kind == QUERY_TASK) {
    std::fprintf(out, "query '%s': found task\n", name);
    print_task(out, cfg.tasks[q.index]);
    return;
  }
}

int run(const char *config, FILE *out) {
  auto const parse_result = parse_config(config);
  if (not parse_result.has_value()) {
    auto const err = parse_result.error();
    std::fprintf(out, "parse error at line %d: %s\n", err.line,
                 err.message.c_str());
    return 1;
  }

  auto const cfg = parse_result.value();

  auto const validate_result = validate_config(cfg);
  if (not validate_result.has_value()) {
    auto const err = validate_result.error();
    std::fprintf(out, "validation error: %s\n", err.message.c_str());
    return 1;
  }

  auto const detect_cycles_result = detect_cycles(cfg);
  if (not detect_cycles_result.has_value()) {
    auto const err = detect_cycles_result.error();
    std::fprintf(out, "cycle error: %s\n", err.message.c_str());
    return 1;
  }

  print_line(out);
  std::fprintf(out, "raw config objects\n");
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
