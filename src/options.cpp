#include "options.hpp"

#include <iostream>

void printIgnoreRuleHelp(std::ostream &out)
{
  out << "Ignore rules:\n"
      << "  build                  that name, at any depth\n"
      << "  build/                 folders only\n"
      << "  *.gen.cpp              wildcards; ? matches one character\n"
      << "  /build                 only at the root of the searched path\n"
      << "  src/generated          a path relative to the searched path\n"
      << "  **/temp_out            at any depth, including the top level\n"
      << "  ./build, ../app/build  a path relative to your current directory\n"
      << "  !keep_me.cpp           put back what an earlier rule excluded\n"
      << "\n"
      << "An --exclude rule that matches nothing is reported on stderr.\n";
}

namespace
{
void printUsage(std::ostream &out, const std::string &usageBody)
{
  out << usageBody << "\n";
  printIgnoreRuleHelp(out);
}
} // namespace

bool parseCommonOptions(int argc, char *argv[], const std::string &toolName,
                        const std::string &usageBody, CommonOptions &options,
                        ParseOutcome &outcome)
{
  outcome = ParseOutcome::Run;

  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--exclude" && i + 1 < argc)
    {
      if (!options.ignoreRules.add(argv[++i], true))
      {
        std::cerr << "Warning: --exclude \"" << argv[i] << "\" is not a usable rule\n";
      }
    }
    else if (arg == "--ext" && i + 1 < argc)
    {
      std::string ext = argv[++i];
      if (ext[0] != '.')
        ext = "." + ext;
      options.extensions.insert(ext);
    }
    else if (arg == "--no-defaults")
    {
      options.extensions.clear();
    }
    else if (arg == "--sort")
    {
      options.sortByCount = true;
    }
    else if (arg == "--gitignore")
    {
      options.useGitignore = true;
    }
    else if (arg == "--json")
    {
      options.asJson = true;
    }
    else if (arg == "--help" || arg == "-h")
    {
      printUsage(std::cout, usageBody);
      outcome = ParseOutcome::Finished;
      return true;
    }
    else if (arg == "--version" || arg == "-v")
    {
      std::cout << toolName << " " << MOOSECOUNT_VERSION << '\n';
      outcome = ParseOutcome::Finished;
      return true;
    }
    else if (arg == "--ignore-file" && i + 1 < argc)
    {
      if (!loadIgnoreFile(argv[++i], options.ignoreRules))
      {
        std::cerr << "Warning: Could not open ignore file: " << argv[i] << '\n';
      }
    }
    else if (arg[0] != '-')
    {
      options.searchPaths.push_back(arg);
    }
    else
    {
      std::cerr << toolName << ": unrecognized option '" << arg << "'\n\n";
      printUsage(std::cerr, usageBody);
      return false;
    }
  }

  if (options.searchPaths.empty())
  {
    options.searchPaths.push_back(".");
  }

  return true;
}
