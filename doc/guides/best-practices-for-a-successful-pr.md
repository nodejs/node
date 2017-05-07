# Best Practices
  <sub>(a.k.a "Rules of thumb")</sub>

## Provide motivation for the change
Try to explain why this change will make the code better. For example, does
it fix a bug, or is it a new feature, etc. This should be expressed in the
commit messages as well as in the PR description.

## Don't make _unnecessary_ code changes
_Unnecessary_ code changes are changes made because of personal preference
or style. For example, renaming of variables or functions, adding or removing
white spaces, and reordering lines or whole code blocks. These sort of
changes should have a good reason since otherwise they cause unnecessary
[code churn][5] As part of the project's strategy we maintain multiple release 
lines, code churn might hinder back-porting changes to other lines. Also when
you change a line, your name will come up in `git blame` and shadow the name of
the previous author of that line.

## Keep the change-set self contained but at a reasonable size
Use good judgment when making a big change. If a reason does not come to mind
but a very big change needs to be made, try to break it into smaller
pieces (still as self-contained as possible), and cross-reference them,
explicitly stating that they are dependent on each other.

## Be aware of our style rules
As part of accepting a PR the changes __must__ pass our linters.
* For C++ we use Google's `cpplint` (with some adjustments) so following their
[style-guide][1] should make your code
compliant with our linter.
* For JS we use this [rule-set][2]
for ESLint plus some of [our own custom rules][3].
* For markdown we have a [style guide][4]

  [1]: https://github.com/google/styleguide   "Google C++ style guide"
  [2]: https://github.com/nodejs/node/blob/master/.eslintrc.yaml  "eslintrc"
  [3]: https://github.com/nodejs/node/tree/master/tools/eslint-rules "Our Rules"
  [4]: https://github.com/nodejs/node/blob/master/doc/STYLE_GUIDE.md "MD Style"
  [5]: https://blog.gitprime.com/why-code-churn-matters   "Why churm matters"
