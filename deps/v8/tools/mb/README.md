# MB - The Meta-Build wrapper

MB is a simple wrapper intended to provide a uniform interface to either
GYP or GN, such that users and bots can call one script and not need to
worry about whether a given bot is meant to use GN or GYP.

It supports two main functions:

1. "gen" - the main `gyp_chromium` / `gn gen` invocation that generates the
   Ninja files needed for the build.

2. "analyze" - the step that takes a list of modified files and a list of
   desired targets and reports which targets will need to be rebuilt.

We also use MB as a forcing function to collect all of the different 
build configurations that we actually support for Chromium builds into
one place, in `//tools/mb/mb_config.pyl`.

For more information, see:

* [The User Guide](docs/user_guide.md)
* [The Design Spec](docs/design_spec.md)
