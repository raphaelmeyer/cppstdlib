#define APPROVALS_DOCTEST
#include "ApprovalTests.hpp"

#include "lib.h"

#include <cstdlib>
#include <string>

static std::string capture_run(const char *config) {
    char *buf = nullptr;
    size_t len = 0;
    FILE *f = open_memstream(&buf, &len);
    run(config, f);
    std::fclose(f);
    std::string result(buf, len);
    std::free(buf);
    return result;
}

TEST_CASE("sample config output") {
    const char *config =
        "package core\n"
        "version 1\n"
        "size 10\n"
        "\n"
        "task build\n"
        "uses core\n"
        "cost 5\n";
    ApprovalTests::Approvals::verify(capture_run(config));
}
