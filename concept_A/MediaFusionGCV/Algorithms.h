#pragma once

#include "Algorithm.h"
#include "AlgorithmParam.h"

#include <memory>
#include <string>
#include <vector>

// Registry of the built-in algorithms. ONE table (in Algorithms.cpp) is the
// source of truth: the factory, the name list the GUI populates its selector
// from, and the parameter schemas all read from it, so adding an algorithm is
// adding a row plus the class it points at.
struct AlgorithmInfo
{
    // Wire name. Lowercase, no spaces — the console stores it in a widget's
    // caption and round-trips it through upper/lower case, so a mixed-case name
    // would not survive the trip back. Hyphens are fine.
    const char* name;

    // One line, shown as the control's tooltip.
    const char* summary;

    // Tunable knobs, or {} for a stage with none. A free function rather than a
    // virtual on purpose: listing the menu must not construct anything, and
    // constructing a detector starts a worker thread.
    std::vector<AlgorithmParam> (*schema)();

    std::unique_ptr<Algorithm> (*make)();
};

// Every algorithm the engine can put in a chain, in menu order.
const std::vector<AlgorithmInfo>& algorithmRegistry();

// Registry lookup by wire name, or nullptr if there is no such algorithm.
const AlgorithmInfo* findAlgorithm(const std::string& name);

// Returns a new Algorithm for `name`, or nullptr if unknown.
std::unique_ptr<Algorithm> makeAlgorithm(const std::string& name);

// Names accepted by makeAlgorithm(), for the GUI to populate its selector.
std::vector<std::string> availableAlgorithms();

// Parameter schema for `name`; empty for an unknown algorithm or one with no
// knobs (the two cases are distinguished by findAlgorithm()).
std::vector<AlgorithmParam> algorithmParams(const std::string& name);
