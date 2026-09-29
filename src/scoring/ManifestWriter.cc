/// \file ManifestWriter.cc
/// \brief Implementation of ManifestWriter

#include "scoring/ManifestWriter.hh"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace
{
  std::string Number(double value)
  {
    if (!std::isfinite(value))
      return "null";
    std::ostringstream stream;
    stream << std::setprecision(12) << value;
    return stream.str();
  }

  std::string Str(const std::string &text)
  {
    return "\"" + ManifestWriter::EscapeJson(text) + "\"";
  }

  std::string Vec3(const double *values)
  {
    return "[" + Number(values[0]) + ", " + Number(values[1]) + ", " + Number(values[2]) + "]";
  }

  void WriteRun(std::ostream &out, const ManifestData::RunRecord &run)
  {
    out << "{\"run\": " << run.runId << ", \"events\": " << run.events;
    if (run.hasBeam)
    {
      out << ", \"particle\": " << Str(run.beam.particle)
          << ", \"beamEnergy_keV\": " << Number(run.beam.energy_keV)
          << ", \"position_um\": " << Vec3(run.beam.position_um)
          << ", \"direction\": " << Vec3(run.beam.direction);
    }
    else
    {
      out << ", \"particle\": null, \"beamEnergy_keV\": null, \"position_um\": null, "
             "\"direction\": null";
    }
    out << ", \"energyDeposit_eV\": " << Number(run.energyDeposit_eV)
        << ", \"seed\": " << run.seed << "}";
  }
} // namespace

std::string ManifestWriter::EscapeJson(const std::string &text)
{
  std::string escaped;
  escaped.reserve(text.size());
  for (const char c : text)
  {
    switch (c)
    {
    case '"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20)
      {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
        escaped += buffer;
      }
      else
      {
        escaped += c;
      }
    }
  }
  return escaped;
}

void ManifestWriter::Write(std::ostream &out, const ManifestData::Manifest &manifest)
{
  long totalEvents = 0;
  double totalEnergy_eV = 0.;
  for (const ManifestData::RunRecord &run : manifest.runs)
  {
    totalEvents += run.events;
    totalEnergy_eV += run.energyDeposit_eV;
  }

  out << "{\n";
  out << "  \"schemaVersion\": " << manifest.schemaVersion << ",\n";
  out << "  \"timestamp\": " << Str(manifest.timestamp) << ",\n";
  out << "  \"geant4Version\": " << Str(manifest.geant4Version) << ",\n";
  out << "  \"macro\": " << Str(manifest.macro) << ",\n";
  out << "  \"chemistry\": " << Str(manifest.chemistry) << ",\n";

  if (manifest.scavengers.empty())
  {
    out << "  \"scavengers\": [],\n";
  }
  else
  {
    out << "  \"scavengers\": [\n";
    for (std::size_t i = 0; i < manifest.scavengers.size(); ++i)
    {
      out << "    {\"species\": " << Str(manifest.scavengers[i].species)
          << ", \"molarity_M\": " << Number(manifest.scavengers[i].molarity_M) << "}"
          << (i + 1 < manifest.scavengers.size() ? "," : "") << "\n";
    }
    out << "  ],\n";
  }

  out << "  \"pH\": " << Number(manifest.pH) << ",\n";
  out << "  \"chemistryEndTime_ns\": " << Number(manifest.chemistryEndTime_ns) << ",\n";
  out << "  \"runMode\": " << Str(manifest.runMode) << ",\n";
  out << "  \"threads\": " << manifest.threads << ",\n";
  out << "  \"outputDirAsConfigured\": " << Str(manifest.outputDirAsConfigured) << ",\n";
  out << "  \"outputDirAbsolute\": " << Str(manifest.outputDirAbsolute) << ",\n";
  out << "  \"prefix\": " << Str(manifest.prefix) << ",\n";
  out << "  \"subdir\": " << Str(manifest.subdir) << ",\n";
  out << "  \"totalEvents\": " << totalEvents << ",\n";
  out << "  \"totalEnergyDeposit_eV\": " << Number(totalEnergy_eV) << ",\n";

  if (manifest.files.empty())
  {
    out << "  \"files\": [],\n";
  }
  else
  {
    out << "  \"files\": [\n";
    for (std::size_t i = 0; i < manifest.files.size(); ++i)
    {
      out << "    " << Str(manifest.files[i]) << (i + 1 < manifest.files.size() ? "," : "")
          << "\n";
    }
    out << "  ],\n";
  }

  if (manifest.runs.empty())
  {
    out << "  \"runs\": []\n";
  }
  else
  {
    out << "  \"runs\": [\n";
    for (std::size_t i = 0; i < manifest.runs.size(); ++i)
    {
      out << "    ";
      WriteRun(out, manifest.runs[i]);
      out << (i + 1 < manifest.runs.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
  }

  out << "}\n";
}
