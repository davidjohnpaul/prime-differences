# Contributing to Prime Differences

Thank you for your interest in contributing to Prime Differences.
This repository accepts contributions from researchers, students, and developers.
Please read this document to understand how to contribute and the expected standards.

## Code of Conduct

The full Contributor Covenant 3.0 Code of Conduct is defined in CODE_OF_CONDUCT.md.
Please read CODE_OF_CONDUCT.md before contributing.

## How to contribute

- Start by opening an issue to discuss proposed changes if you’re unsure.
- Submit a pull request against the main branch with a clear description of the change.
- Include tests for new functionality where feasible.

## Coding standards

- Language: C++ (C++23)
- Naming: follow the existing codebase conventions (internal utilities in src/; tests in tests/).
- Commit messages: use an imperative style (e.g., "Add feature X", "Fix bug in Y").

### Pre-commit formatting checks

We use a pre-commit hook to enforce formatting consistency with clang-format and our .clang-format configuration.

#### What you need to do

1) Install prerequisites
- Ensure Python 3.x is installed.
- Install pre-commit:
  ```bash
  python3 -m pip install --user pre-commit
  ```

2) Install the git hook
```bash
pre-commit install
```

3) Run checks on all files (optional but recommended for a clean slate)
```bash
pre-commit run --all-files
```

#### What happens when you commit

- The pre-commit hook runs clang-format using the repository’s .clang-format.
- If any files are reformatted, the hook will stage the changes. You’ll need to review and recommit.

#### How to proceed if you want to check formatting manually

- You can format specific files with clang-format:
  ```bash
  clang-format -i path/to/file.cpp path/to/file.hpp
  ```
- Or run the hook on all files:
  ```bash
  pre-commit run --all-files
  ```

## Testing and CI

- This project uses GitHub Actions to build and test on Linux, macOS, and Windows.
- You should build and run tests locally:
  - cmake -B build -DCMAKE_BUILD_TYPE=Release
  - cmake --build build --parallel
  - ctest --output-on-failure
- Add unit tests for new functionality; tests live under tests/.

## Documentation

- Update or add documentation as needed in docs/ or README.md; ensure usage is clear for readers.

## Licensing and attribution

- This project is licensed under the BSD-2-Clause license. See LICENSE for details.
- If you incorporate third-party code, ensure you comply with its license and provide attribution as required.

## Contact

- For questions or to discuss contributions, use the project’s Issues page or contact the maintainers via email.
