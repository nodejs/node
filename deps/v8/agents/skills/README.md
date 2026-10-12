# Agent Skills

This directory contains specialized Agent Skills for V8 development.

Unlike general context files, skills are shared, "on-demand" expertise that AI
agents (such as Jetski, Claude, GitHub Copilot, etc.) can activate when relevant
to your request.

## How to Use

To use skills in Jetski, install them into your workspace `.agents/skills/`
directory via symlinks so they stay up-to-date when you sync your local
checkout:

```bash
vpython3 agents/scripts/install_for_jetski.py
```

Once installed, your agent will automatically detect when a skill is relevant to
your request and read its `SKILL.md` instructions.

## Contributing

New skills should be self-contained within their own directory under
`agents/skills/`. Each skill requires a `SKILL.md` file at its root with a name
and description in the YAML frontmatter.
