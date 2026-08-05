#pragma once

#include <map>
#include <string>
#include <vector>

// A tunable knob on an Algorithm, described well enough that a client can build
// a control for it without knowing which algorithm it belongs to.
//
// Every value is a double — a bool is 0/1 and an enum is an index into
// `choices`. One numeric type keeps the wire format a single shape
// (`key=value`), and it is what a slider produces anyway. The `type` field is
// what tells a GUI to render a checkbox or a combo instead of a slider.
//
// The schema is static per algorithm: it is served by a free function in the
// registry, not a virtual, so listing the knobs never has to construct an
// algorithm (constructing a detector starts a worker thread).
struct AlgorithmParam
{
    enum class Type { Int, Float, Bool, Enum };

    std::string              key;      // wire name, lowercase: "low", "min-area"
    std::string              label;    // human caption: "LOW THRESHOLD"
    Type                     type = Type::Float;
    double                   min  = 0.0;
    double                   max  = 1.0;
    double                   step = 0.01;
    double                   def  = 0.0;
    std::vector<std::string> choices;  // Enum only, indexed by the value

    // Bring a value into range. Int/Bool/Enum additionally snap to whole
    // numbers, so a client that sends 0.7 for a bool gets 1, not a third state.
    double clamp(double v) const;
};

// Parameter values as they travel: algorithm-local, keyed by AlgorithmParam::key.
using AlgorithmParams = std::map<std::string, double>;

// Defaults for a schema, for a caller that wants a complete value set.
AlgorithmParams defaultParams(const std::vector<AlgorithmParam>& schema);
