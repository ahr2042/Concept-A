#include "AlgorithmParam.h"

#include <algorithm>
#include <cmath>

double AlgorithmParam::clamp(double v) const
{
    if (std::isnan(v))
        return def;

    double out = std::min(std::max(v, min), max);
    if (type != Type::Float)
        out = std::round(out);
    return out;
}

AlgorithmParams defaultParams(const std::vector<AlgorithmParam>& schema)
{
    AlgorithmParams out;
    for (const auto& p : schema)
        out[p.key] = p.def;
    return out;
}
