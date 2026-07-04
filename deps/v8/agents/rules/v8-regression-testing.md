---
name: v8-regression-testing-rule
trigger: glob
globs: test/mjsunit/**/*regress*
---

# Regression Test Skill Enforcement

If you create or modify a regression test in `test/mjsunit/` with "regress" in
its name, you MUST use the `v8-regression-testing` skill.
