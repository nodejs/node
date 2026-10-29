# V8 Coding Agents

This directory provides a centralized location for files related to AI coding
agents (e.g. `jetski-cli`) used for development within V8.

The goal is to provide a scalable and organized way to share prompts and tools
among developers, accommodating the various environments (Linux, Mac, Windows)
and agent types in use.

To ensure consistency across the ecosystem, the directory structure and
documentation standards (excluding V8-specific skills and plugins) mirror those
in
[Chromium's agents directory](https://source.chromium.org/chromium/chromium/src/+/main:agents/).

## Directory Structure

### Skills & Rules

Reusable workflows, commands, and conventions live in `skills/` and `rules/`.

To install them for your agent environment, run the appropriate installation
script from the `agents/` directory:

- **For Jetski**: Run `vpython3 scripts/install_for_jetski.py` to create
  symlinks in `.agents/`.
- **For GitHub Copilot CLI**: Run `vpython3 scripts/install_for_copilot_cli.py`
  to generate repository instructions in `.github/copilot-instructions.md`,
  install compatible V8 rules as instruction files in `.github/instructions/`,
  and expose compatible V8 skills in `.github/skills/`.

## Contributing

Please freely add self-contained agent skills that match the format of the
existing examples.
