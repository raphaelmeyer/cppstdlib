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

TEST_CASE("validation error") {
  const char *config = "package core\n"
                       "version 1\n";
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
