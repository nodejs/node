# Redline agent instructions

Read the documentation in this directory before you change, review or test code
in `experimental/redline/`:

- [README.md](README.md): what Redline is, the quick start (`out/fuzzbuild/d8`
  and `out/redline`), the directory layout and the code conventions.
- [DESIGN.md](DESIGN.md): the architecture, the stages and the design rules.
- [TUTORIAL.md](TUTORIAL.md): the commands, from the build to a fuzzing session
  with your own configuration.
- [CONTRIBUTE.md](CONTRIBUTE.md): how to add or change a component. Section 10
  tells how to measure a change, and section 11 is the checklist for a CL. Read
  this file before you change or review code.

Before you upload a change to `experimental/redline/`, do the checks of the
[redline-presubmit](skills/redline-presubmit/SKILL.md) skill. Put its report in
the CL description.
