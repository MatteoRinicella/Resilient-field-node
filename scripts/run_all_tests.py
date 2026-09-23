from pathlib import Path
import subprocess
import sys

project_directory = Path(__file__).resolve().parents[1]
tests_directory = project_directory / "tests"

test_files = sorted(
    tests_directory.rglob("test_*.py"),
)

passed_count = 0
failed_failes = []

for test_file in test_files:
    relative_test_path = test_file.relative_to(
        project_directory,
    )

    print()
    print(
        "Esecuzione:",
        relative_test_path
    )

    result = subprocess.run(
        [
            sys.executable,
            str(test_file),
        ],
        cwd=project_directory,
    )

    if result.returncode == 0:
        passed_count += 1
    else:
        failed_failes.append(
            relative_test_path,
        )


print()
print(
    "Test superati:",
    passed_count,
    "/",
    len(test_files),
)

if failed_failes:
    print("Test falliti:")

    for failed_file in failed_failes:
        print(
            "-",
            failed_file,
        )

    raise SystemExit(1)

print("Tutti i test sono stati superati")
