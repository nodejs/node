# Prompts

This directory contains the system instruction prompt template
(`templates/modular.md`) for V8 coding agents (`jetski-cli`).

## Creating the System Instruction Prompt

Run `vpython3 agents/scripts/install_for_jetski.py` to generate your root
`GEMINI.md` file from `agents/prompts/templates/modular.md` and link `.agents/`
rules and skills.

## Contributing

Changes to `templates/modular.md` should be done *carefully* as it is meant to
be used broadly across V8 workspaces.
