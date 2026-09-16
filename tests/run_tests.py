import subprocess
import re
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BIN_PATH = os.path.join(PROJECT_ROOT, "bin", "moosecount")

# The exact totals we expect from analyzing tests/data/
EXPECTED_TOTALS = {
    "File Count": 1,
    "Code Lines": 4,
    "Format Lines": 1,
    "Comment Lines": 4,
    "Blank Lines": 2,
    "Total Lines": 11,
    "Lines of Code": 5
}

def run_raw(args, cwd=None):
    """Runs the binary and hands back the finished process, whatever it did."""
    return subprocess.run(
        [BIN_PATH] + args,
        capture_output=True, text=True, cwd=cwd or PROJECT_ROOT
    )

def run_moosecount(args, cwd=None):
    """Runs the binary and returns (stdout, parsed totals)."""
    result = run_raw(args, cwd)

    if result.returncode != 0:
        print("Error: moosecount execution failed.")
        print("Stderr:", result.stderr)
        sys.exit(1)

    totals = {}

    # Regex to capture lines like "Code Lines      = 45"
    pattern = re.compile(r"([\w\s]+)\s+=\s+(\d+)")
    for line in result.stdout.splitlines():
        match = pattern.match(line)
        if match:
            key = match.group(1).strip()
            value = int(match.group(2))
            totals[key] = value

    return result.stdout, totals

def run_moosecount_stderr(args, cwd=None):
    """Runs the binary and returns stderr, ignoring the counting output."""
    return run_raw(args, cwd).stderr

def counted_files(output, base_path):
    """Extracts the per-file listing as paths relative to base_path."""
    files = set()
    for line in output.splitlines():
        if "\t" not in line:
            continue
        count, path = line.split("\t", 1)
        if not count.strip().isdigit():
            continue
        files.add(os.path.relpath(path.strip(), base_path).replace(os.sep, "/"))
    return files

def check(label, actual, expected):
    if actual != expected:
        print(f"FAIL: '{label}' expected {expected}, but got {actual}")
        return False
    print(f"PASS: '{label}' == {expected}")
    return True

def test_help_and_version():
    """Verifies the conventions a caller expects from --help and --version.

    Both are a successful request for information: they go to stdout and exit
    0. An unrecognized flag is the opposite on every count.
    """
    print("\n--- Help and version ---")
    passed = True

    for flag in ["--help", "-h"]:
        result = run_raw([flag])
        if result.returncode != 0:
            print(f"FAIL: '{flag}' exited {result.returncode}, expected 0")
            passed = False
        elif "Usage:" not in result.stdout:
            print(f"FAIL: '{flag}' should print usage to stdout")
            passed = False
        elif result.stderr:
            print(f"FAIL: '{flag}' should leave stderr empty, got: {result.stderr.strip()}")
            passed = False
        else:
            print(f"PASS: '{flag}' prints usage to stdout and exits 0")

    for flag in ["--version", "-v"]:
        result = run_raw([flag])
        if result.returncode != 0:
            print(f"FAIL: '{flag}' exited {result.returncode}, expected 0")
            passed = False
        elif not re.match(r"^moosecount \d+\.\d+\.\d+$", result.stdout.strip()):
            print(f"FAIL: '{flag}' printed {result.stdout.strip()!r}, expected 'moosecount X.Y.Z'")
            passed = False
        else:
            print(f"PASS: '{flag}' reports {result.stdout.strip()}")

    # The version has to come from CMake, not a copy that drifts
    declared = None
    with open(os.path.join(PROJECT_ROOT, "CMakeLists.txt")) as handle:
        match = re.search(r"project\s*\([^)]*VERSION\s+(\d+\.\d+\.\d+)", handle.read())
        if match:
            declared = match.group(1)

    reported = run_raw(["--version"]).stdout.strip().split()[-1]
    if declared is None:
        print("FAIL: CMakeLists.txt declares no project VERSION")
        passed = False
    elif declared != reported:
        print(f"FAIL: CMakeLists.txt says {declared}, binary says {reported}")
        passed = False
    else:
        print(f"PASS: version matches CMakeLists.txt ({declared})")

    result = run_raw(["--bogus"])
    if result.returncode == 0:
        print("FAIL: an unrecognized flag should not exit 0")
        passed = False
    elif "Usage:" not in result.stderr:
        print("FAIL: an unrecognized flag should print usage to stderr")
        passed = False
    elif result.stdout:
        print(f"FAIL: an unrecognized flag should leave stdout empty, got: {result.stdout.strip()}")
        passed = False
    else:
        print("PASS: an unrecognized flag exits non-zero and explains itself on stderr")

    return passed

def test_basic_totals():
    """Verifies the line classification counters against tests/data/."""
    print("\n--- Counting totals ---")
    data_path = os.path.join(SCRIPT_DIR, "data")
    _, actual_totals = run_moosecount([data_path])

    passed = True
    for key, expected_val in EXPECTED_TOTALS.items():
        if not check(key, actual_totals.get(key), expected_val):
            passed = False
    return passed

def test_ignore_file_patterns():
    """Verifies --ignore-file honors gitignore-style wildcards like '*build*/'."""
    print("\n--- Ignore file patterns ---")
    data_path = os.path.join(SCRIPT_DIR, "data_ignore")
    ignore_path = os.path.join(data_path, "gitignore_sample")

    # Without an ignore file, all three fixture files are counted
    _, baseline = run_moosecount([data_path])
    passed = check("File Count (no ignore file)", baseline.get("File Count"), 3)

    # 'vendor_libs/' is an exact name, '*build*/' must match 'prebuild_out'
    output, filtered = run_moosecount(["--ignore-file", ignore_path, data_path])
    if not check("File Count (with ignore file)", filtered.get("File Count"), 1):
        passed = False

    for excluded in ["prebuild_out", "vendor_libs"]:
        if excluded in output:
            print(f"FAIL: '{excluded}' should have been skipped, but appears in the output")
            passed = False
        else:
            print(f"PASS: '{excluded}' was skipped")

    if "kept.cpp" not in output:
        print("FAIL: 'kept.cpp' should have been counted, but is missing from the output")
        passed = False
    else:
        print("PASS: 'kept.cpp' was counted")

    return passed

def test_ignore_file_paths():
    """Verifies anchored ('/foo/') and path-prefix ('src/foo/') ignore rules."""
    print("\n--- Ignore file paths ---")
    data_path = os.path.join(SCRIPT_DIR, "data_paths")
    ignore_path = os.path.join(data_path, "gitignore_sample")

    # Without an ignore file, all six fixture files are counted
    _, baseline = run_moosecount([data_path])
    passed = check("File Count (no ignore file)", baseline.get("File Count"), 6)

    output, filtered = run_moosecount(["--ignore-file", ignore_path, data_path])
    if not check("File Count (with ignore file)", filtered.get("File Count"), 3):
        passed = False

    skipped = [
        "data_paths/anchored_root/skipped.cpp",   # '/anchored_root/' anchors to the root
        "data_paths/src/generated/skipped.cpp",   # 'src/generated/' is a path prefix
        "data_paths/deep/nest/temp_out/skipped.cpp"  # '**/temp_out/' spans directories
    ]
    counted = [
        "data_paths/kept.cpp",
        "data_paths/nested/anchored_root/kept.cpp",  # anchored rule must not match here
        "data_paths/other/generated/kept.cpp"        # different prefix, must not match
    ]

    for path in skipped:
        if path in output:
            print(f"FAIL: '{path}' should have been skipped, but appears in the output")
            passed = False
        else:
            print(f"PASS: '{path}' was skipped")

    for path in counted:
        if path not in output:
            print(f"FAIL: '{path}' should have been counted, but is missing from the output")
            passed = False
        else:
            print(f"PASS: '{path}' was counted")

    return passed

def test_exclude_rule_forms():
    """Verifies the ways a user might spell the same --exclude rule.

    A leading '/' anchors to the root of the search path; a bare name stays
    unanchored and matches at any depth. Path-shaped rules ('./x', '../x') are
    covered separately, since those resolve against the working directory.
    """
    print("\n--- Exclude rule spellings ---")
    data_path = os.path.join(SCRIPT_DIR, "data_paths")
    nested_twin = "nested/anchored_root/kept.cpp"

    passed = True

    # Anchored: only the copy at the root of the search path goes
    for rule in ["/anchored_root", "/anchored_root/"]:
        output, totals = run_moosecount(["--exclude", rule, data_path])
        if not check(f"File Count ('{rule}')", totals.get("File Count"), 5):
            passed = False
        elif nested_twin not in counted_files(output, data_path):
            print(f"FAIL: '{rule}' should not have reached '{nested_twin}'")
            passed = False

    # Unanchored: a bare name matches at any depth, so both copies go
    for rule in ["anchored_root", "anchored_root/"]:
        _, totals = run_moosecount(["--exclude", rule, data_path])
        if not check(f"File Count ('{rule}')", totals.get("File Count"), 4):
            passed = False

    return passed

def test_cwd_relative_rules():
    """Verifies that './x' and '../x' resolve against the working directory.

    These are the spellings a shell produces, and the whole point is being able
    to exclude a folder inside a tree that sits above where you are standing.
    """
    print("\n--- Working directory relative rules ---")
    data_path = os.path.join(SCRIPT_DIR, "data_paths")
    ignore_path = os.path.join(SCRIPT_DIR, "data_ignore")
    nested_twin = "nested/anchored_root/kept.cpp"
    passed = True

    # Standing in the tree: './x' excludes the copy at the top of it
    for rule in ["./anchored_root", "./anchored_root/"]:
        output, totals = run_moosecount(["--exclude", rule, "."], cwd=data_path)
        if not check(f"File Count ('{rule}' from inside the tree)", totals.get("File Count"), 5):
            passed = False
        elif nested_twin not in counted_files(output, "."):
            print(f"FAIL: '{rule}' should not have reached '{nested_twin}'")
            passed = False

    # Standing elsewhere: a '../' rule pointing into the searched tree still works
    output, totals = run_moosecount(
        ["--exclude", "../data_paths/anchored_root", "../data_paths"], cwd=ignore_path)
    if not check("File Count ('../data_paths/anchored_root' from a sibling)",
                 totals.get("File Count"), 5):
        passed = False
    elif nested_twin not in counted_files(output, "../data_paths"):
        print(f"FAIL: the '../' rule should not have reached '{nested_twin}'")
        passed = False

    # A rule pointing outside the searched tree excludes nothing, and says so
    output, totals = run_moosecount(["--exclude", "../data_ignore", "."], cwd=data_path)
    if not check("File Count (rule outside the tree)", totals.get("File Count"), 6):
        passed = False

    stderr = run_moosecount_stderr(["--exclude", "../data_ignore", "."], cwd=data_path)
    if "outside the paths being searched" not in stderr:
        print(f"FAIL: expected an 'outside' warning, got: {stderr.strip() or '(nothing)'}")
        passed = False
    else:
        print("PASS: a rule outside the searched tree is reported as such")

    return passed

def test_unmatched_rule_warning():
    """Verifies that an --exclude rule which matched nothing is reported.

    Rules read out of an ignore file stay quiet: a shared .gitignore listing
    folders that happen not to exist in this tree is normal, not a mistake.
    """
    print("\n--- Unmatched rule warning ---")
    data_path = os.path.join(SCRIPT_DIR, "data_paths")
    shared_ignore = os.path.join(SCRIPT_DIR, "data_ignore", "gitignore_sample")
    passed = True

    def expect_quiet(label, args, cwd=None):
        stderr = run_moosecount_stderr(args, cwd=cwd)
        if stderr.strip():
            print(f"FAIL: {label} should have been quiet, got: {stderr.strip()}")
            return False
        print(f"PASS: {label} is quiet")
        return True

    def expect_warning(label, args, needle):
        stderr = run_moosecount_stderr(args)
        if needle not in stderr:
            print(f"FAIL: {label} should have mentioned '{needle}', got: {stderr.strip() or '(nothing)'}")
            return False
        print(f"PASS: {label} warns")
        return True

    if not expect_warning("a rule matching nothing",
                          ["--exclude", "no_such_folder", data_path], "no_such_folder"):
        passed = False

    if not expect_warning("a rule that cannot be parsed",
                          ["--exclude", "/", data_path], "not a usable rule"):
        passed = False

    if not expect_quiet("a rule that did its job", ["--exclude", "anchored_root", data_path]):
        passed = False

    # The './' spelling used to match nothing silently; it must now be quiet
    if not expect_quiet("the './' spelling",
                        ["--exclude", "./anchored_root/", "."], cwd=data_path):
        passed = False

    # gitignore_sample's rules match nothing in data_paths, and that is fine
    if not expect_quiet("rules read from a file", ["--ignore-file", shared_ignore, data_path]):
        passed = False

    return passed

def test_nested_ignore_files():
    """Verifies that a .gitignore inside a subfolder applies to that subfolder.

    This is git's own behavior and the case that shows up with submodules: rules
    in module_a/.gitignore are scoped to module_a and are relative to it, while
    the top-level .gitignore still applies to the whole tree. Discovery is opt-in
    via --gitignore; --ignore-file still reads exactly the one file it is given.
    """
    print("\n--- Nested ignore files ---")
    data_path = os.path.join(SCRIPT_DIR, "data_nested")

    expected_counted = {
        "kept.cpp",
        "module_a/kept.cpp",
        "module_a/deeper/local_build/kept.cpp",  # '/local_build/' anchors to module_a
        "module_b/kept.cpp",
        "module_b/generated/kept.cpp",           # module_a's rules must not leak here
        "module_b/root_only/kept.cpp",           # '/root_only/' anchors to the tree root
        "module_b/x_tmp/kept.cpp"                # module_a/deeper's rule is dropped on the way out
    }
    expected_skipped = {
        "vendor/skipped.cpp",                    # top-level rule, any depth
        "module_b/vendor/skipped.cpp",           # top-level rule reaches into module_b
        "root_only/skipped.cpp",                 # top-level anchored rule
        "module_a/generated/skipped.cpp",        # module_a's own rule
        "module_a/deeper/generated/skipped.cpp", # module_a's rule, any depth below it
        "module_a/local_build/skipped.cpp",      # module_a's anchored rule
        "module_a/deeper/x_tmp/skipped.cpp"      # third-level rule, scoped to module_a/deeper
    }

    _, baseline = run_moosecount([data_path])
    passed = check("File Count (no ignore file)", baseline.get("File Count"),
                   len(expected_counted) + len(expected_skipped))

    output, filtered = run_moosecount(["--gitignore", data_path])
    if not check("File Count (with --gitignore)", filtered.get("File Count"), len(expected_counted)):
        passed = False

    actual = counted_files(output, data_path)

    for path in sorted(expected_skipped & actual):
        print(f"FAIL: '{path}' should have been skipped, but appears in the output")
        passed = False
    for path in sorted(expected_counted - actual):
        print(f"FAIL: '{path}' should have been counted, but is missing from the output")
        passed = False

    if passed:
        print("PASS: nested ignore files are scoped and anchored correctly")

    return passed

def test_negation_and_file_rules():
    """Verifies '!' negation and rules that match files rather than folders.

    Ordering is what matters here: within one file the last matching rule wins,
    and a deeper .gitignore outranks the one above it. A negation still cannot
    rescue a file whose folder was excluded, which is git's behavior too.
    """
    print("\n--- Negation and file rules ---")
    data_path = os.path.join(SCRIPT_DIR, "data_negate")
    ignore_path = os.path.join(data_path, ".gitignore")

    expected_counted = {
        "plain.cpp",
        "keep_me.gen.cpp",          # '*.gen.cpp' then '!keep_me.gen.cpp'
        "module/plain.cpp",
        "module/drop_me.gen.cpp"    # rescued by module/.gitignore
    }
    expected_skipped = {
        "drop_me.gen.cpp",          # file-level '*.gen.cpp'
        "generated/inside.cpp",     # inside the excluded folder
        "generated/special.cpp"     # negated, but its folder is excluded
    }

    _, baseline = run_moosecount([data_path])
    passed = check("File Count (no ignore file)", baseline.get("File Count"),
                   len(expected_counted) + len(expected_skipped))

    output, filtered = run_moosecount(["--gitignore", data_path])
    if not check("File Count (with --gitignore)", filtered.get("File Count"), len(expected_counted)):
        passed = False

    actual = counted_files(output, data_path)
    for path in sorted(expected_skipped & actual):
        print(f"FAIL: '{path}' should have been skipped, but appears in the output")
        passed = False
    for path in sorted(expected_counted - actual):
        print(f"FAIL: '{path}' should have been counted, but is missing from the output")
        passed = False

    if passed:
        print("PASS: negation and file-level rules resolve in order")

    # --ignore-file reads only the file it is given, so the nested rescue is absent
    output, single = run_moosecount(["--ignore-file", ignore_path, data_path])
    if not check("File Count (--ignore-file, no nested rescue)", single.get("File Count"), 3):
        passed = False
    elif "module/drop_me.gen.cpp" in counted_files(output, data_path):
        print("FAIL: 'module/drop_me.gen.cpp' should need module/.gitignore to be counted")
        passed = False
    else:
        print("PASS: --ignore-file applies only the rules it was handed")

    return passed

def run_integration_test():
    results = [
        test_help_and_version(),
        test_basic_totals(),
        test_ignore_file_patterns(),
        test_ignore_file_paths(),
        test_exclude_rule_forms(),
        test_cwd_relative_rules(),
        test_unmatched_rule_warning(),
        test_nested_ignore_files(),
        test_negation_and_file_rules()
    ]

    # Known gaps go here: behavior we want but have not implemented yet. They
    # report without failing the suite. Move one up into `results` once it passes.
    pending = []

    if not all(results):
        sys.exit(1)

    print("\nAll integration tests passed successfully!")
    for name, ok in pending:
        status = "now passing, promote it to a required test" if ok else "not yet implemented"
        print(f"KNOWN GAP ({status}): {name}")

if __name__ == "__main__":
    run_integration_test()
