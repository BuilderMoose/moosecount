import subprocess
import re
import sys
import os

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

def run_integration_test():
    # Find the root of the project (assuming script is in tests/)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    bin_path = os.path.join(project_root, "bin", "moosecount")
    data_path = os.path.join(script_dir, "data")
    
    # 1. Run moosecount against the test data directory
    result = subprocess.run(
        [bin_path, data_path],
        capture_output=True, text=True, cwd=project_root
    )
    
    if result.returncode != 0:
        print("Error: moosecount execution failed.")
        print("Stderr:", result.stderr)
        sys.exit(1)

    # 2. Parse the output
    output = result.stdout
    actual_totals = {}
    
    # Regex to capture lines like "Code Lines      = 45"
    pattern = re.compile(r"([\w\s]+)\s+=\s+(\d+)")
    for line in output.splitlines():
        match = pattern.match(line)
        if match:
            key = match.group(1).strip()
            value = int(match.group(2))
            actual_totals[key] = value

    # 3. Verify the output matches expectations
    passed = True
    for key, expected_val in EXPECTED_TOTALS.items():
        actual_val = actual_totals.get(key)
        if actual_val != expected_val:
            print(f"FAIL: '{key}' expected {expected_val}, but got {actual_val}")
            passed = False
        else:
            print(f"PASS: '{key}' == {expected_val}")

    if not passed:
        sys.exit(1)
    
    print("\nAll integration tests passed successfully!")

if __name__ == "__main__":
    run_integration_test()
