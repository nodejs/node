# Prompts

This directory contains the common prompt for V8 and template prompts to teach
agents about specific tools. Everything is intended to work with gemini-cli.

## Creating the System Instruction Prompt

Create a local, untracked `GEMINI.md` file in your root directory. Include
relevant prompts using the `@` syntax. For example:

```GEMINI.md
@agents/prompts/common.md
```

You can confirm that prompts were successfully imported by running the `/memory
show` command in gemini-cli.

## Known problems

All imports must be scoped to the current prompt file. a/prompt.md can import
a/prompt2.md or a/b/prompt3.md, but cannot import c/prompt4.md. See
https://github.com/google-gemini/gemini-cli/issues/4098.

## Contributing

Changes to `common.md`  should be done *carefully* as it's meant to be used broadly.
Use the `template/` directory to add new prompts that you want to share.
