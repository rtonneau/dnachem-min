/// \file SpeciesSampleTimes.cc
/// \brief Implementation of the species sample-time grids (see the header).

#include "scoring/SpeciesSampleTimes.hh"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace SpeciesSampleTimes
{
  namespace
  {
    constexpr double kPicosecond = 1e-3; // ns
    constexpr double kMaxPrecision = 1e-2; // ns (10 ps)
    constexpr double kRelTol = 1e-9;

    // Time of a unit name in ns, or a negative value if the unit is unknown.
    double UnitValue(const std::string& unit)
    {
      if (unit == "picosecond" || unit == "ps") return 1e-3;
      if (unit == "nanosecond" || unit == "ns") return 1.;
      if (unit == "microsecond" || unit == "us") return 1e3;
      if (unit == "millisecond" || unit == "ms") return 1e6;
      if (unit == "second" || unit == "s") return 1e9;
      return -1.;
    }

    void SortUnique(std::vector<double>& v)
    {
      std::sort(v.begin(), v.end());
      v.erase(std::unique(v.begin(), v.end()), v.end());
    }

    // True when t is strictly below endTime and not within kRelTol of it.
    bool BelowEnd(double t, double endTime)
    {
      return t < endTime * (1. - kRelTol);
    }

    std::vector<double> DecadePoints()
    {
      return {1e-3, 1e-2, 0.1, 1., 10., 100., 1000.};
    }
  } // namespace

  bool ParseList(const std::string& text, std::vector<double>& timesOut,
                 std::string& error)
  {
    timesOut.clear();
    error.clear();

    std::istringstream in(text);
    std::vector<std::string> tokens;
    for (std::string tok; in >> tok;) tokens.push_back(tok);

    if (tokens.empty()) {
      error = "empty time list";
      return false;
    }
    if (tokens.size() < 2) {
      error = "expected '<t1> ... <tN> <unit>', got '" + text + "'";
      return false;
    }

    const std::string& unit = tokens.back();
    const double unitValue = UnitValue(unit);
    if (unitValue < 0.) {
      error = "unknown time unit '" + unit + "'";
      return false;
    }

    std::vector<double> times;
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
      const std::string& tok = tokens[i];
      char* end = nullptr;
      const double value = std::strtod(tok.c_str(), &end);
      if (end == tok.c_str() || *end != '\0' || !std::isfinite(value)) {
        error = "cannot parse '" + tok + "' as a number";
        return false;
      }
      if (value <= 0.) {
        error = "time must be > 0, got '" + tok + "'";
        return false;
      }
      times.push_back(value * unitValue);
    }

    SortUnique(times);
    timesOut = times;
    return true;
  }

  std::vector<double> BuildGrid(const Setting& s, double endTime,
                                std::vector<double>* droppedOut)
  {
    if (droppedOut != nullptr) droppedOut->clear();

    std::vector<double> grid;
    switch (s.mode) {
      case Mode::Default:
        for (double t : DecadePoints())
          if (BelowEnd(t, endTime)) grid.push_back(t);
        grid.push_back(endTime);
        break;

      case Mode::Fixed:
        if (s.step > 0.) {
          for (long long k = 1;; ++k) {
            const double t = static_cast<double>(k) * s.step;
            if (!BelowEnd(t, endTime)) break;
            grid.push_back(t);
          }
        }
        grid.push_back(endTime);
        break;

      case Mode::PerDecade:
        if (s.perDecade > 0) {
          for (long long k = 0;; ++k) {
            const double t =
              kPicosecond * std::pow(10., static_cast<double>(k) / s.perDecade);
            if (!BelowEnd(t, endTime)) break;
            grid.push_back(t);
          }
        }
        grid.push_back(endTime);
        break;

      case Mode::List:
        for (double t : s.list) {
          if (t <= endTime) {
            grid.push_back(t);
          }
          else if (droppedOut != nullptr) {
            droppedOut->push_back(t);
          }
        }
        if (droppedOut != nullptr) SortUnique(*droppedOut);
        break;
    }

    SortUnique(grid);
    return grid;
  }

  std::map<double, double> PrecisionMap(const Setting& s)
  {
    std::map<double, double> precision;

    switch (s.mode) {
      case Mode::Fixed:
        precision[DBL_MAX] = std::min(kMaxPrecision, s.step / 10.);
        break;

      case Mode::PerDecade: {
        const double ratio = std::pow(10., 1. / s.perDecade) - 1.;
        double low = kPicosecond;
        for (int decade = 0; decade < 9; ++decade, low *= 10.) {
          precision[low * 10.] = std::min(kMaxPrecision, low * ratio / 10.);
        }
        precision[DBL_MAX] = kMaxPrecision;
        break;
      }

      case Mode::Default:
      case Mode::List: {
        std::vector<double> points =
          (s.mode == Mode::Default) ? DecadePoints() : s.list;
        SortUnique(points);
        for (std::size_t i = 0; i + 1 < points.size(); ++i) {
          precision[points[i + 1]] =
            std::min(kMaxPrecision, (points[i + 1] - points[i]) / 10.);
        }
        precision[DBL_MAX] = kMaxPrecision;
        break;
      }
    }
    return precision;
  }

  bool Validate(const Setting& s, std::string& error)
  {
    error.clear();
    switch (s.mode) {
      case Mode::Fixed:
        if (!(s.step > 0.)) {
          error = "fixed step must be > 0";
          return false;
        }
        break;
      case Mode::PerDecade:
        if (s.perDecade <= 0) {
          error = "points per decade must be > 0";
          return false;
        }
        break;
      case Mode::List:
        if (s.list.empty()) {
          error = "time list is empty";
          return false;
        }
        break;
      case Mode::Default:
        break;
    }
    return true;
  }
} // namespace SpeciesSampleTimes
