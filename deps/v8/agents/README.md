# V8 Coding Agents

This directory provides a centralized location for files related to AI coding
agents (e.g. `gemini-cli`) used for development within V8.

The goal is to provide a scalable and organized way to share prompts and tools
among developers, accommodating the various environments (Linux, Mac, Windows)
and agent types in use.

To ensure consistency across the ecosystem, the directory structure and
documentation standards (excluding V8-specific skills and extensions) mirror
those in
[Chromium's agents directory](https://source.chromium.org/chromium/chromium/src/+/main:agents/).

## Directory Structure

### Subagents

On-demand expertise for specific tasks. Source files are located in
[`agents/`](agents/).

To use them, you need to run the appropriate installation script from the
`agents/` directory:

- **For Gemini CLI**: Run `vpython3 scripts/install_for_gemini_cli.py` to
  generate the subagent files in `.gemini/agents/`.
- **For Jetski**: Run `vpython3 scripts/install_for_jetski.py` to create
  symlinks in `.agents/agents/`.

## Contributing

Please freely add self-contained agent skills that match the format of the
existing examples.
