npm-completion(1) -- Tab Completion for npm
===========================================

## SYNOPSIS

    source <(npm completion)

## DESCRIPTION

Enables tab-completion in all npm commands.

The synopsis above
loads the completions into your current shell.  Adding it to
your ~/.bashrc or ~/.zshrc will make the completions available
everywhere:

    npm completion >> ~/.bashrc
    npm completion >> ~/.zshrc

You may of course also pipe the output of npm completion to a file
such as `/usr/local/etc/bash_completion.d/npm` if you have a system
that will read that file for you.

When `COMP_CWORD`, `COMP_LINE`, and `COMP_POINT` are defined in the
environment, `npm completion` acts in "plumbing mode", and outputs
completions based on the arguments.

## SEE ALSO

* npm-developers(7)
* npm(1)
