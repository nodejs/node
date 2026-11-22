# minimatch

A glob matching utility.

This is the matching library used internally by
[node-glob](https://npm.im/node-glob).

It works by splitting glob expressions on `/`, and testing each
path portion with a `Matcher` from
[@isaacs/nonrecursive-glob-match](http://npm.im/@isaacs/nonrecursive-glob-match).

## Why Not Regular Expressions?

Regular Expressions are very powerful, and for many applications,
extremely fast.

However, in JavaScript, they are subject to
[ReDoS](https://owasp.org/www-community/attacks/Regular_expression_Denial_of_Service_-_ReDoS)
in certain cases. Libraries that assemble Regular Expressions
from arbitrary input are thus vulnerable to patterns that cause
pathological behavior. For example, patterns like
`a*a*a*a*a*a*a*a*a*a*a` when matched against a string like
`'a'.repeat(50) + 'b'` would have to repeatedly backtrack through
every failure.

Most other glob matchers in JavaScript (including versions up
through v10 of this library) convert the glob pattern to Regular
Expressions, and then use that to test the string. This is fine
for trusted input, but can fall over badly in cases that cause
recursive backtracking.

## Important Security Consideration!

Though this library is _not_ vulnerable to ReDOS, _any_ library
that performs pattern matching based on arbitrary input will
potentially have edge cases where it performs suboptimally.

To be extremely clear:

> [!WARNING]  
> **If you create a system where you take user input, and use
> that input as the source of a pattern matcher, in this or any
> extant glob matcher in JavaScript, you may be pwned.**

The pattern matching library that Minimatch uses in v11 and above
will not be backported to earlier versions. It is a much too deep
of a change to do so.

## Usage

```ts
// hybrid module, load with require() or import
import { minimatch } from 'minimatch'
// or:
// const { minimatch } = require('minimatch')

minimatch('bar.foo', '*.foo') // true!
minimatch('bar.foo', '*.bar') // false!
minimatch('bar.foo', '*.+(bar|foo)') // true!
```

## Features

Supports these glob features:

- Brace Expansion
- Extended glob matching
- "Globstar" `**` matching
- [Posix character
  classes](https://www.gnu.org/software/bash/manual/html_node/Pattern-Matching.html),
  like `[[:alpha:]]`, supporting the full range of Unicode
  characters. For example, `[[:alpha:]]` will match against
  `'é'`, though `[a-zA-Z]` will not. Collating symbol and set
  matching is not supported, so `[[=e=]]` will _not_ match `'é'`
  and `[[.ch.]]` will not match `'ch'` in locales where `ch` is
  considered a single character.

See:

- `man sh`
- `man bash` [Pattern
  Matching](https://www.gnu.org/software/bash/manual/html_node/Pattern-Matching.html)
- `man 3 fnmatch`
- `man 5 gitignore`

## Windows

**Please only use forward-slashes in glob expressions.**

Though windows uses either `/` or `\` as its path separator, only `/`
characters are used by this glob implementation. You must use
forward-slashes **only** in glob expressions. Back-slashes in patterns
will always be interpreted as escape characters, not path separators.

Note that `\` or `/` _will_ be interpreted as path separators in paths on
Windows, and will match against `/` in glob expressions.

So just always use `/` in patterns.

### UNC Paths

On Windows, UNC paths like `//?/c:/...` or
`//ComputerName/Share/...` are handled specially.

- Patterns starting with a double-slash followed by some
  non-slash characters will preserve their double-slash. As a
  result, a pattern like `//*` will match `//x`, but not `/x`.
- Patterns staring with `//?/<drive letter>:` will _not_ treat
  the `?` as a wildcard character. Instead, it will be treated
  as a normal string.
- Patterns starting with `//?/<drive letter>:/...` will match
  file paths starting with `<drive letter>:/...`, and vice versa,
  as if the `//?/` was not present. This behavior only is
  present when the drive letters are a case-insensitive match to
  one another. The remaining portions of the path/pattern are
  compared case sensitively, unless `nocase:true` is set.
- The host and share of a pattern starting with
  `//<host>/<share>/...` are compared case-insensitively with
  those of the file path, if neither has magic. For example,
  `//host/share/*` matches `//HOST/SHARE/x`.
- Patterns starting with `//?/UNC/<host>/<share>/...` will match
  file paths starting with `//<host>/<share>/...`, and vice
  versa, the same as `//?/<drive letter>:`. The `?` is not a
  wildcard character.

Note that specifying a UNC path using `\` characters as path
separators is always allowed in the file path argument, but only
allowed in the pattern argument when `windowsPathsNoEscape: true`
is set in the options.

## Minimatch Class

Create a minimatch object by instantiating the `minimatch.Minimatch` class.

```javascript
import { Minimatch } from 'minimatch'
const mm = new Minimatch(pattern, options)
```

### Properties

- `pattern` The original pattern the minimatch object represents.
- `options` The options supplied to the constructor.
- `set` A 2-dimensional array of `Matcher` or string expressions.
  Each row in the
  array corresponds to a brace-expanded pattern. Each item in the row
  corresponds to a single path-part. For example, the pattern
  `{a,b/c}/d` would expand to a set of patterns like:

        [ [ a, d ]
        , [ b, c, d ] ]

  If a portion of the pattern doesn't have any "magic" in it
  (that is, it's something like `"foo"` rather than `fo*o?`), then it
  will be left as a string rather than converted to a `Matcher`.

  A numeric range of 100 or more values, such as `{0..999}`, is
  not expanded. It stays in one row, and the `Matcher` for its
  portion tests for a number in the range.

- `negate` True if the pattern is negated.
- `comment` True if the pattern is a comment.
- `empty` True if the pattern is `""`.

### Methods

- `match(fname)` Return true if the filename matches the pattern, or
  false otherwise.
- `hasMagic()` Returns true if the parsed pattern contains any
  magic characters. Returns false if all comparator parts are
  string literals. If the `magicalBraces` option is set on the
  constructor, then it will consider brace expansions which are
  not otherwise magical to be magic. If not set, then a pattern
  like `a{b,c}d` will return `false`, because neither `abd` nor
  `acd` contain any special glob characters.

  This does **not** mean that the pattern string can be used as a
  literal filename, as it may contain magic glob characters that
  are escaped. For example, the pattern `\\*` or `[*]` would not
  be considered to have magic, as the matching portion parses to
  the literal string `'*'` and would match a path named `'*'`,
  not `'\\*'` or `'[*]'`. The `minimatch.unescape()` method may
  be used to remove escape characters.

All other methods are internal, and will be called as necessary.

### minimatch(path, pattern, options)

Main export. Tests a path against the pattern using the options.

```javascript
const isJS = minimatch(file, '*.js', { matchBase: true })
```

It keeps at least the 256 `Minimatch` objects that it used most
recently, so a pattern that it tests again with the same options
is not compiled again. To test many paths against one pattern, a
`Minimatch` object or `minimatch.filter()` is still a bit faster.

### minimatch.filter(pattern, options)

Returns a function that tests its
supplied argument, suitable for use with `Array.filter`. Example:

```javascript
const javascripts = fileList.filter(
  minimatch.filter('*.js', { matchBase: true }),
)
```

### minimatch.escape(pattern, options = {})

Escape all magic characters in a glob pattern, so that it will
only ever match literal strings.

If the `windowsPathsNoEscape` option is used, then characters are
escaped by wrapping in `[]`, because a magic character wrapped in
a character class can only be satisfied by that exact character.

Braces (`{` and `}`) are only escaped if the `magicalBraces`
option is set. Without it, a brace expansion in the input such as
`{a,b}` is left as-is, and will be expanded when matching.

A leading `!` or `#` is escaped, so that the result is never a
negated pattern or a comment. In `windowsPathsNoEscape` mode, a
leading `!` is written as `@(!)`, which only matches `!` (and
matches nothing if `noext` is set).

Slashes (and backslashes in `windowsPathsNoEscape` mode) cannot
be escaped or unescaped.

### minimatch.unescape(pattern, options = {})

Un-escape a glob string that may contain some escaped characters.

If the `windowsPathsNoEscape` option is used, then square-brace
escapes are removed, but not backslash escapes. For example, it
will turn the string `'[*]'` into `*`, but it will not turn
`'\\*'` into `'*'`, because `\` is a path separator in
`windowsPathsNoEscape` mode.

When `windowsPathsNoEscape` is not set, then both brace escapes
and backslash escapes are removed.

Slashes (and backslashes in `windowsPathsNoEscape` mode) cannot
be escaped or unescaped.

### minimatch.match(list, pattern, options)

Match against the list of
files, in the style of fnmatch or glob. If nothing is matched, and
options.nonull is set, then return a list containing the pattern itself.

```javascript
const javascripts = minimatch.match(fileList, '*.js', { matchBase: true })
```

## Options

All options are `false` by default.

### nobrace

Do not expand `{a,b}` and `{1..3}` brace sets.

### noglobstar

Disable `**` matching against multiple folder names.

### dot

Allow patterns to match filenames starting with a period, even if
the pattern does not explicitly have a period in that spot.

Note that by default, `a/**/b` will **not** match `a/.d/b`, unless `dot`
is set.

### noext

Disable "extglob" style patterns like `+(a|b)`.

### nocase

Perform a case-insensitive match. Case is folded one code point at
a time, with Unicode simple case folding. See [Comparisons to other
fnmatch/glob
implementations](#comparisons-to-other-fnmatchglob-implementations).

### nocaseMagicOnly

When used with `{nocase: true}`, create matchers that
are case-insensitive, but leave string match portions untouched.
Has no effect when used without `{nocase: true}`.

Useful when some other form of case-insensitive matching is used,
or if the original string representation is useful in some other
way.

### normalize

Normalize unicode in the pattern, and in each portion of the tested
path, with this form: `'NFC'`, `'NFD'`, `'NFKC'`, or `'NFKD'`. `true`
means `'NFC'`.

A char such as `é` can be one code point, or `e` followed by a
combining accent. Some file systems, such as on macOS, can give file
names in the second form. Without this option, the two forms do not
match.

- The path is split on `/` before it is normalized. So with NFKC, a
  full width `／` normalizes to `/`, but it does not split the path.
  It is a char in its portion.
- The syntax of the pattern is parsed before it is normalized. So
  with NFKC, a full width `＊` is a literal, which matches `*`, and
  not a wildcard.
- With `nocase`, the strings are normalized, and then case is folded.

See [@isaacs/nonrecursive-glob-match](http://npm.im/@isaacs/nonrecursive-glob-match#unicode-normalization)
for how classes and combining marks after wildcards are normalized.

### nonull

When a match is not found by `minimatch.match`, return a list containing
the pattern itself if this option is set. When not set, an empty list
is returned if there are no matches.

### magicalBraces

This only affects the results of the `Minimatch.hasMagic` method.

If the pattern contains brace expansions, such as `a{b,c}d`, but
no other magic characters, then the `Minimatch.hasMagic()` method
will return `false` by default. When this option set, it will
return `true` for brace expansion as well as other magic glob
characters.

### matchBase

If set, then patterns without slashes will be matched
against the basename of the path if it contains slashes. For example,
`a?b` would match the path `/xyz/123/acb`, but not `/xyz/acb/123`.

### nocomment

Suppress the behavior of treating `#` at the start of a pattern as a
comment.

### nonegate

Suppress the behavior of treating a leading `!` character as negation.

### flipNegate

Returns from negate expressions the same as if they were not negated.
(Ie, true on a hit, false on a miss.)

### partial

Compare a partial path to a pattern. As long as the parts of the path that
are present are not contradicted by the pattern, it will be treated as a
match. This is useful in applications where you're walking through a
folder structure, and don't yet have the full path, but want to ensure that
you do not walk down paths that can never be a match.

For example,

```js
minimatch('/a/b', '/a/*/c/d', { partial: true }) // true, might be /a/b/c/d
minimatch('/a/b', '/**/d', { partial: true }) // true, might be /a/b/.../d
minimatch('/x/y/z', '/a/**/z', { partial: true }) // false, because x !== a
```

### windowsPathsNoEscape

Use `\\` as a path separator _only_, and _never_ as an escape
character. If set, all `\\` characters are replaced with `/` in
the pattern. Note that this makes it **impossible** to match
against paths containing literal glob pattern characters, but
allows matching with patterns constructed using `path.join()` and
`path.resolve()` on Windows platforms, mimicking the (buggy!)
behavior of earlier versions on Windows. Please use with
caution, and be mindful of [the caveat about Windows
paths](#windows).

For legacy reasons, this is also set if
`options.allowWindowsEscape` is set to the exact value `false`.

### windowsNoMagicRoot

When a pattern starts with a UNC path or drive letter, and in
`nocase:true` mode, do not convert the root portions of the
pattern into a case-insensitive matchers, and instead
leave them as strings.

This is the default when the platform is `win32` and
`nocase:true` is set.

### preserveMultipleSlashes

By default, multiple `/` characters (other than the leading `//`
in a UNC path, see "UNC Paths" above) are treated as a single
`/`.

That is, a pattern like `a///b` will match the file path `a/b`.

Set `preserveMultipleSlashes: true` to suppress this behavior.

### optimizationLevel

A number indicating the level of optimization that should be done
to the pattern prior to parsing and using it for matches.

Globstar parts `**` are always converted to `*` when `noglobstar`
is set, and multiple adjacent `**` parts are converted into a
single `**` (ie, `a/**/**/b` will be treated as `a/**/b`, as this
is equivalent in all cases).

- `0` - Make no further changes. In this mode, `.` and `..` are
  maintained in the pattern, meaning that they must also appear
  in the same position in the test path string. Eg, a pattern
  like `a/*/../c` will match the string `a/b/../c` but not the
  string `a/c`.
- `1` - (default) Remove cases where a double-dot `..` follows a
  pattern portion that is not `**`, `.`, `..`, or empty `''`. For
  example, the pattern `./a/b/../*` is converted to `./a/*`, and
  so it will match the path string `./a/c`, but not the path
  string `./a/b/../c`. Dots and empty path portions in the
  pattern are preserved. On Windows, a `..` never removes the
  drive letter or the UNC host and share at the start of a
  pattern, the same as it never removes the `/` at the start of
  a posix pattern. So `C:/a/../../x` is converted to `C:/../x`.
- `2` (or higher) - Much more aggressive optimizations, suitable
  for use with file-walking cases:
  - Remove cases where a double-dot `..` follows a pattern
    portion that is not `**`, `.`, or empty `''`, or a Windows
    root as above. Remove empty
    and `.` portions of the pattern, where safe to do so (ie,
    anywhere other than the last position, the first position, or
    the second position in a pattern starting with `/`, as this
    may indicate a UNC path on Windows).
  - Convert patterns containing `<pre>/**/../<p>/<rest>` into the
    equivalent `<pre>/{..,**}/<p>/<rest>`, where `<p>` is a
    a pattern portion other than `.`, `..`, `**`, or empty
    `''`. If this would make more than `braceExpandMax` patterns,
    or add more than 1,000,000 path portions in total,
    construction throws a `RangeError`.
  - Remove duplicate patterns.
  - Remove a pattern when another pattern has `*` at one or more
    positions, and the same portions at all other positions. The
    portions that `*` replaces must be non-dot patterns other than
    `**` or `''`. So `a/{*,x}/b` becomes `a/*/b`, because `*` can
    match against `x`. This step is skipped when the number of
    patterns times the number of different sets of `*` positions
    is more than 100,000.

  While these optimizations improve the performance of
  file-walking use cases such as [glob](http://npm.im/glob) (ie,
  the reason this module exists), there are cases where it will
  fail to match a literal string that would have been matched in
  optimization level 1 or 0.

### platform

When set to `win32`, this will trigger all windows-specific
behaviors (special handling for UNC paths, and treating `\` as
separators in file paths for comparison.)

Defaults to the value of `process.platform`.

### maxGlobstarRecursion

Max number of non-adjacent `**` patterns to recursively walk
down.

The default of `200` is almost certainly high enough for most
purposes, and can handle absurdly excessive patterns.

If the limit is exceeded (which would require very excessively
long patterns and paths containing lots of `**` patterns!), then
it is treated as non-matching, even if the path would normally
match the pattern provided.

That is, this is an intentional false negative, deemed an
acceptable break in correctness for security and performance.

## Comparisons to other fnmatch/glob implementations

While strict compliance with the existing standards is a
worthwhile goal, some discrepancies exist between minimatch and
other implementations. Some are intentional, and some are
unavoidable.

If the pattern starts with a `!` character, then it is negated. Set the
`nonegate` flag to suppress this behavior, and treat leading `!`
characters normally. This is perhaps relevant if you wish to start the
pattern with a negative extglob pattern like `!(a|B)`. Multiple `!`
characters at the start of a pattern will negate the pattern multiple
times.

If a pattern starts with `#`, then it is treated as a comment, and
will not match anything. Use `\#` to match a literal `#` at the
start of a line, or set the `nocomment` flag to suppress this behavior.

The double-star character `**` is supported by default, unless the
`noglobstar` flag is set. This is supported in the manner of bsdglob
and bash 4.1, where `**` only has special significance if it is the only
thing in a path part. That is, `a/**/b` will match `a/x/y/b`, but
`a/**b` will not.

If an escaped pattern has no matches, and the `nonull` flag is set,
then minimatch.match returns the pattern as-provided, rather than
interpreting the character escapes. For example,
`minimatch.match([], "\\*a\\?")` will return `"\\*a\\?"` rather than
`"*a?"`. This is like bash with the `nullglob` option off (the
default), except that bash also removes the escapes.

If brace expansion is not disabled, then it is performed before any
other interpretation of the glob pattern. Thus, a pattern like
`+(a|{b),c)}`, which would not be valid in bash or zsh, is expanded
**first** into the set of `+(a|b)` and `+(a|c)`, and those patterns are
checked for validity. Since those two are valid, matching proceeds.

A numeric range of 100 or more values, such as `{0..999}` or
`{001..500}`, is the exception. It is not expanded, but it matches
the same strings as its expansion. A range is expanded as usual if
it has a step, such as `{0..999..2}`, if a `\` or `$` is before
it, if a `[` is before it in the same path portion, because it
might be inside a class, or if it is inside a `!(...)`, `*(...)`
or `+(...)` extglob, because those repeat or negate each value.

Each portion of a pattern between `/` characters is matched with
[@isaacs/nonrecursive-glob-match](http://npm.im/@isaacs/nonrecursive-glob-match),
which uses Bash 5 as its reference, and does not use regular
expressions. So negated extglobs such as `<start>!(<pattern>*|)*`
match the same strings as in Bash. These are the known
differences:

- If `dot` is not set, a portion that starts with `.` only matches
  if a literal `.` in the pattern matches that first character.
  Extglobs before that `.` can match the empty string, so the
  pattern can still match. For example, with `nonegate`,
  `!(.a|js)@(.*)` matches `.a`, `.a.js`, and `.js`, because
  `!(.a|js)` matches the empty string. Bash does not match these
  dotfiles.
- If an extglob such as `?(a` is not closed, Bash matches the
  whole pattern as a literal string. In this library, only the
  `?(` is literal, and the rest of the pattern is still matched
  as a glob. So `a?(b*` matches `a?(bcc` here, but not in Bash.
- A pattern is split into portions on every `/`, including a `/`
  inside an extglob. So `@(a|b/c)/*` becomes the portions
  `@(a|b`, `c)`, and `*`, and its extglob is not closed. Bash
  keeps the extglob whole, and drops each alternative with a `/`
  in it, as no portion can contain a `/`. So in Bash, `@(a|b/c)/*`
  is the same as `@(a)/*`. Leave out the alternatives with a `/`
  to get the same result here.
- With `nocase`, case is folded one code point at a time with
  Unicode simple case folding, the same as a JavaScript RegExp
  with the `iu` flags. Bash uses the case functions of the C
  library for the current locale, so its results can change with
  the locale.

Note that `fnmatch(3)` in libc is an extremely naive string comparison
matcher, which does not do anything special for slashes. This library is
designed to be used in glob searching and file walkers, and so it does do
special things with `/`. Thus, `foo*` will not match `foo/bar` in this
library, even though it would in `fnmatch(3)`.
