# Tips, Tricks & Troubleshooting

## Rules of thumb

<details>
<summary><strong>Provide motivation for the change</strong></summary>
Try to explain why this change will make the code better. For example, does
it fix a bug, or is it a new feature, etc. This should be expressed in the
commit messages as well as in the PR description.
</details>
<details>
<summary><strong>Don't make <em>unnecessary</em> code changes</strong></summary>
<em>Unnecessary</em> code changes are changes made because of personal preference
or style. For example, renaming of variables or functions, adding or removing
white spaces, and reordering lines or whole code blocks. These sort of
changes should have a good reason since otherwise they cause unnecessary
<a href="https://blog.gitprime.com/why-code-churn-matters">"code churn"</a>.
As part of the project's strategy we maintain multiple release lines, code
churn might hinder back-porting changes to other lines. Also when you
change a line, your name will come up in `git blame` and shadow the name of
the previous author of that line.
</details>
<details>
<summary><strong>Keep the change-set self contained but at a reasonable size</strong></summary>
Use good judgment when making a big change. If a reason doesn't come to mind
but a very big change needs to be made, try to break it into smaller
pieces (still as self-contained as possible), and cross-reference them,
explicitly stating that they are dependent on each other.
</details>
<details>
<summary><strong>Be aware of our style rules</strong></summary>
As part of accepting a PR the changes <strong>must</strong> pass our linters.
<ul>
<li>For C++ we use Google's `cpplint` (with some adjustments) so following their
<a href="https://github.com/google/styleguide">style-guide</a> should make your code
compliant with our linter.</li>
<li>For JS we use this
<a hreaf="https://github.com/nodejs/node/blob/master/.eslintrc.yaml">rule-set</a>
for ESLint plus some of
<a href="https://github.com/nodejs/node/tree/master/tools/eslint-rules">our own custom rules</a>.</li>
<li>For markdown we have a <a href="https://github.com/nodejs/node/blob/master/doc/STYLE_GUIDE.md">style guide</a></li>
</ul>
</details>


## Tips

### Windows

1. If you regularly build on windows, you should check out [`ninja`]. You may
   find it to be much faster than `MSBuild` and less eager to rebuild unchanged
   sub-targets.
   P.S. non-windows developers may also find it better then `make`.

   [`ninja`]: ./building-node-with-ninja.md

<!--
## Tricks

TDB

-->
## Troubleshooting

1. Python crashes with `LookupError: unknown encoding: cp65001`
   This is a known `Windows`/`python` bug ([_python bug_][1],
   [_stack overflow_][2]). The simplest solution is to set an
   environment variable which tells `python` how to handle this situation:
   ```cmd
   set PYTHONIOENCODING=UTF-8
   ```

   [1]: http://bugs.python.org/issue1602    "python bug"
   [2]: http://stackoverflow.com/questions/878972/windows-cmd-encoding-change-causes-python-crash    "stack overflow"



