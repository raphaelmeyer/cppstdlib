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
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n"
                       "cost 5\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("parse error") {
  ApprovalTests::Approvals::verify(capture_run("package\n"));
}

TEST_CASE("duplicate package") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "package core\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("task missing name") {
  ApprovalTests::Approvals::verify(capture_run("task\n"));
}

TEST_CASE("duplicate task") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n"
                       "cost 5\n"
                       "\n"
                       "task build\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("version no arg") {
  const char *config = "package core\n"
                       "version\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("invalid version") {
  const char *config = "package core\n"
                       "version abc\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("size no arg") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("invalid size") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size abc\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("depends no names") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "depends\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("feature multiple names") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "feature a b\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("unknown package directive") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "badkey\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("uses no package") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("cost no arg") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n"
                       "cost\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("invalid cost") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n"
                       "cost abc\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("requires no tasks") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n"
                       "cost 5\n"
                       "requires\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("unknown task directive") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n"
                       "cost 5\n"
                       "badkey\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("directive outside block") {
  ApprovalTests::Approvals::verify(capture_run("version 1\n"));
}

TEST_CASE("validation error") {
  const char *config = "package core\n"
                       "version 1\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("package missing version") {
  const char *config = "package core\n"
                       "size 10\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("unknown package dependency") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "depends unknown\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("task missing uses") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "cost 5\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("task missing cost") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("task uses unknown package") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses nonexistent\n"
                       "cost 5\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("task requires unknown task") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task build\n"
                       "uses core\n"
                       "cost 5\n"
                       "requires nonexistent\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("package cycle") {
  const char *config = "package a\n"
                       "version 1\n"
                       "size 10\n"
                       "depends b\n"
                       "\n"
                       "package b\n"
                       "version 1\n"
                       "size 10\n"
                       "depends a\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("task cycle") {
  const char *config = "package core\n"
                       "version 1\n"
                       "size 10\n"
                       "\n"
                       "task a\n"
                       "uses core\n"
                       "cost 1\n"
                       "requires b\n"
                       "\n"
                       "task b\n"
                       "uses core\n"
                       "cost 1\n"
                       "requires a\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}

TEST_CASE("full sample") {
  const char *config = "package core\n"
                       "version 3\n"
                       "size 120\n"
                       "\n"
                       "package util\n"
                       "version 2\n"
                       "size 40\n"
                       "depends core\n"
                       "\n"
                       "package net\n"
                       "version 5\n"
                       "size 70\n"
                       "depends core util\n"
                       "\n"
                       "package app\n"
                       "version 7\n"
                       "size 200\n"
                       "depends util net\n"
                       "feature gui\n"
                       "feature cli\n"
                       "\n"
                       "task smoke\n"
                       "uses app\n"
                       "cost 15\n"
                       "\n"
                       "task package\n"
                       "uses app\n"
                       "cost 25\n"
                       "requires smoke\n"
                       "\n"
                       "task integration\n"
                       "uses net\n"
                       "cost 30\n"
                       "requires smoke\n";
  ApprovalTests::Approvals::verify(capture_run(config));
}
