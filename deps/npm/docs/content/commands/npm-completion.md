---
title: npm-completion
section: 1
description: Tab Completion for npm
---

### Synopsis

```bash
npm completion
```

Note: This command is unaware of workspaces.

### Description

Enables tab-completion in all npm commands.

The synopsis above
loads the completions into your current shell.  Adding it to
your ~/.bashrc or ~/.zshrc will make the completions available
everywhere:

```bash
npm completion >> ~/.bashrc
npm completion >> ~/.zshrc
```

You may of course also pipe the output of `npm completion` to a file
such as `/usr/local/etc/bash_completion.d/npm` or 
`/etc/bash_completion.d/npm` if you have a system that will read 
that file for you.

When `COMP_CWORD`, `COMP_LINE`, and `COMP_POINT` are defined in the
environment, `npm completion` acts in "plumbing mode", and outputs
completions based on the arguments.

### See Also

* [npm developers](/using-npm/developers)
* [npm](/commands/npm)
