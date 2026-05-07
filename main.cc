#include "lib.h"

#include <string>

namespace {

std::string const sample_config_text = R"(
# sample package/task configuration

package core
version 3
size 120

package util
version 2
size 40
depends core

package net
version 5
size 70
depends core util

package app
version 7
size 200
depends util net
feature gui
feature cli

task smoke
uses app
cost 15

task package
uses app
cost 25
requires smoke

task integration
uses net
cost 30
requires smoke
)";

}

int main() { return run(sample_config_text, stdout); }
