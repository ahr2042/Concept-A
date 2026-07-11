#pragma once

#include "Algorithm.h"
#include <memory>
#include <string>
#include <vector>

// Factory + registry for the built-in algorithms. Add an entry here (and in
// Algorithms.cpp) when you add a new Algorithm — e.g. an ONNX-based detector.

// Returns a new Algorithm for `name`, or nullptr if unknown.
std::unique_ptr<Algorithm> makeAlgorithm(const std::string& name);

// Names accepted by makeAlgorithm(), for the GUI to populate its selector.
std::vector<std::string> availableAlgorithms();
