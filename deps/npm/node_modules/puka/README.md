# Puka

[![GitLab CI pipeline status](https://gitlab.com/rhendric/puka/badges/master/pipeline.svg)](https://gitlab.com/rhendric/puka/commits/master) [![AppVeyor build status](https://img.shields.io/appveyor/ci/rhendric/puka.svg?label=windows%20tests)](https://ci.appveyor.com/project/rhendric/puka) [![Codecov status](https://img.shields.io/codecov/c/gl/rhendric/puka.svg)](https://codecov.io/gl/rhendric/puka)

Puka is a cross-platform library for safely passing strings through shells.

#### Contents

-   [Introduction](#introduction)
    -   [Why would I use Puka?](#why-would-i-use-puka)
    -   [How do I use Puka?](#how-do-i-use-puka)
    -   [What's the catch?](#whats-the-catch)
-   [API Documentation](#api-documentation)
    -   [Basic API](#basic-api)
        -   [sh](#sh)
        -   [unquoted](#unquoted)
    -   [Advanced API](#advanced-api)
        -   [quoteForShell](#quoteforshell)
        -   [quoteForCmd](#quoteforcmd)
        -   [quoteForSh](#quoteforsh)
        -   [ShellString](#shellstring)
    -   [Secret API](#secret-api)
-   [The sh DSL](#the-sh-dsl)
    -   [Syntax](#syntax)
    -   [Semantics](#semantics)
        -   [Types of placeholders](#types-of-placeholders)

## Introduction

### Why would I use Puka?

When launching a child process from Node, you have a choice between launching
directly from the operating system (as with [child_process.spawn](https://nodejs.org/api/child_process.html#child_process_child_process_spawn_command_args_options),
if you don't use the `{ shell: true }` option), and running the command through
a shell (as with [child_process.exec](https://nodejs.org/api/child_process.html#child_process_child_process_exec_command_options_callback)).
Using a shell gives you more power, such as the ability to chain multiple
commands together or use redirection, but you have to construct your command as
a single string instead of using an array of arguments. And doing that can be
buggy (if not dangerous) if you don't take care to quote any arguments
correctly for the shell you're targeting, _and_ the quoting has to be done
differently on Windows and non-Windows shells.

Puka solves that problem by giving you a simple and platform-agnostic way to
build shell commands with arguments that pass through your shell unaltered and
with no unsafe side effects, **whether you are running on Windows or a
Unix-based OS**.

### How do I use Puka?

Puka gives you an `sh` function intended for tagging
[template literals](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Template_literals),
which quotes (if necessary) any values interpolated into the template. A simple
example:

```javascript
const { sh } = require('puka');
const { execSync } = require('child_process');

const arg = 'file with spaces.txt';
execSync(sh`some-command ${arg}`);
```

But Puka supports more than this! See [the `sh` DSL documentation](#the-sh-dsl)
for a detailed description of all the features currently supported.

### What's the catch?

Here are the ones I know about:

Puka does _not_ ensure that the actual commands you're running are
cross-platform. If you're running npm programs, you generally won't have a
problem with that, but if you want to run ``sh`cat file` `` on Windows, you'll
need to depend on something like
[cash-cat](https://www.npmjs.com/package/cash-cat).

I searched for days for a way to quote or escape line breaks in arguments to
`cmd.exe`, but couldn't find one (regular `^`-prepending and quotation marks
don't seem to cut it). If you know of a way that works, please [open an
issue](https://gitlab.com/rhendric/puka/issues/new) to tell me about it! Until
then, any line break characters (`\r` or `\n`) in values being interpolated by
`sh` will cause an error to be thrown on Windows only.

Also on Windows, you may notice quoting mistakes if you run commands that
involve invoking a native executable (not a batch file ending in `.cmd` or
`.bat`). Unfortunately, batch files require some extra escaping on Windows, and
Puka assumes all programs are batch files because npm creates batch file shims
for programs it installs (and, if you care about cross-platform, you'll be
using npm programs in your commands). If this causes problems for you, please
[open an issue](https://gitlab.com/rhendric/puka/issues/new); if your situation
is specific enough, there may be workarounds or improvements to Puka to be
found.

## API Documentation

### Basic API




#### sh

A string template tag for safely constructing cross-platform shell commands.

An `sh` template is not actually treated as a literal string to be
interpolated; instead, it is a tiny DSL designed to make working with shell
strings safe, simple, and straightforward. To get started quickly, see the
examples below. [More detailed documentation][1] is available
further down.

##### Examples

```javascript
const title = '"this" & "that"';
sh`script --title=${title}`; // => "script '--title=\"this\" & \"that\"'"
// Note: these examples show results for non-Windows platforms.
// On Windows, the above would instead be
// 'script ^^^"--title=\\^^^"this\\^^^" ^^^& \\^^^"that\\^^^"^^^"'.

const names = ['file1', 'file 2'];
sh`rimraf ${names}.txt`; // => "rimraf file1.txt 'file 2.txt'"

const cmd1 = ['cat', 'file 1.txt', 'file 2.txt'];
const cmd2 = ['use-input', '-abc'];
sh`${cmd1}|${cmd2}`; // => "cat 'file 1.txt' 'file 2.txt'|use-input -abc"
```

Returns **[String][2]** a string formatted for the platform Node is currently
running on.

#### unquoted

This function permits raw strings to be interpolated into a `sh` template.

**IMPORTANT**: If you're using Puka due to security concerns, make sure you
don't pass any untrusted content to `unquoted`. This may be obvious, but
stray punctuation in an `unquoted` section can compromise the safety of the
entire shell command.

##### Parameters

-   `value`  any value (it will be treated as a string)

##### Examples

```javascript
const both = true;
sh`foo ${unquoted(both ? '&&' : '||')} bar`; // => 'foo && bar'
```

### Advanced API

If these functions make life easier for you, go ahead and use them; they
are just as well supported as the above. But if you aren't certain you
need them, you probably don't.


#### quoteForShell

Quotes a string for injecting into a shell command.

This function is exposed for some hypothetical case when the `sh` DSL simply
won't do; `sh` is expected to be the more convenient option almost always.
Compare:

```javascript
console.log('cmd' + args.map(a => ' ' + quoteForShell(a)).join(''));
console.log(sh`cmd ${args}`); // same as above

console.log('cmd' + args.map(a => ' ' + quoteForShell(a, true)).join(''));
console.log(sh`cmd "${args}"`); // same as above
```

Additionally, on Windows, `sh` checks the entire command string for pipes,
which subtly change how arguments need to be quoted. If your commands may
involve pipes, you are strongly encouraged to use `sh` and not try to roll
your own with `quoteForShell`.

##### Parameters

-   `text` **[String][2]** to be quoted
-   `forceQuote` **[Boolean][3]?** whether to always add quotes even if the string
    is already safe. Defaults to `false`.
-   `platform` **[String][2]?** a value that `process.platform` might take:
    `'win32'`, `'linux'`, etc.; determines how the string is to be formatted.
    When omitted, effectively the same as `process.platform`.

Returns **[String][2]** a string that is safe for the current (or specified)
platform.

#### quoteForCmd

A Windows-specific version of [quoteForShell][4].

##### Parameters

-   `text` **[String][2]** to be quoted
-   `forceQuote` **[Boolean][3]?** whether to always add quotes even if the string
    is already safe. Defaults to `false`.

#### quoteForSh

A Unix-specific version of [quoteForShell][4].

##### Parameters

-   `text` **[String][2]** to be quoted
-   `forceQuote` **[Boolean][3]?** whether to always add quotes even if the string
    is already safe. Defaults to `false`.

#### ShellString

A ShellString represents a shell command after it has been interpolated, but
before it has been formatted for a particular platform. ShellStrings are
useful if you want to prepare a command for a different platform than the
current one, for instance.

To create a ShellString, use `ShellString.sh` the same way you would use
top-level `sh`.

##### toString

A method to format a ShellString into a regular String formatted for a
particular platform.

###### Parameters

-   `platform` **[String][2]?** a value that `process.platform` might take:
    `'win32'`, `'linux'`, etc.; determines how the string is to be formatted.
    When omitted, effectively the same as `process.platform`.

Returns **[String][2]** 

##### sh

`ShellString.sh` is a template tag just like `sh`; the only difference is
that this function returns a ShellString which has not yet been formatted
into a String.

Returns **[ShellString][5]** 

### Secret API

Some internals of string formatting have been exposed for the ambitious and
brave souls who want to try to extend Puka to handle more shells or custom
interpolated values. This ‘secret’ API is partially documented in the code
but not here, and the semantic versioning guarantees on this API are bumped
down by one level: in other words, minor version releases of Puka can change
the secret API in backward-incompatible ways, and patch releases can add or
deprecate functionality.

If it's not even documented in the code, use at your own risk—no semver
guarantees apply.


[1]: #the-sh-dsl

[2]: https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/String

[3]: https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Boolean

[4]: #quoteforshell

[5]: #shellstring

## The sh DSL

### Syntax

An `sh` template comprises words, separated by whitespace. Words can contain:

-   text, which is composed of any characters that are not whitespace, single or
    double quotes, or any of the special characters
    ``# $ & ( ) ; < > \ ` |``;
-   quotations, which are matching single or double quotes surrounding any
    characters other than the delimiting quote; and
-   placeholders, using the standard JavaScript template syntax (`${}`).
    (Placeholders may also appear inside quotations.)

The special characters ``# $ & ( ) ; < > \ ` |``, if unquoted, form their own
words.

Redirect operators (`<`, `>`, `>>`, `2>`, etc.) receive their own special
handling, as do semicolons. Other than these two exceptions, no attempt is made
to understand any more sophisticated features of shell syntax.

Standard JavaScript escape sequences, such as `\t`, are honored in the template
literal, and are treated equivalently to the characters they represent. There
is no further mechanism for escaping within the `sh` DSL itself; in particular,
if you want to put quotes inside quotes, you have to use interpolation, like
this:

```javascript
sh`echo "${'single = \', double = "'}"` // => "echo 'single = '\\'', double = \"'"
```

### Semantics

Words that do not contain placeholders are emitted mostly verbatim to the
output string. Quotations are formatted in the expected style for the target
platform (single quotes for Unix, double quotes for Windows) regardless of the
quotes used in the template literal—as with JavaScript, single and double quotes
are interchangeable, except for the requirement to pair like with like. Unquoted
semicolons are translated to ampersands on Windows; all other special characters
(as enumerated above), when unquoted, are passed as-is to the output for the
shell to interpret.

Puka may still quote words not containing the above special characters, if they
contain characters that need quoting on the target platform. For example, on
Windows, the character `%` is used for variable interpolation in `cmd.exe`, and
Puka quotes it on on that platform even if it appears unquoted in the template
literal. Consequently, there is no need to be paranoid about quoting anything
that doesn't look alphanumeric inside a `sh` template literal, for fear of being
burned on a different operating system; anything that matches the definition of
‘text’ above will never need manual quoting.

#### Types of placeholders

##### Strings

If a word contains a string placeholder, then the value of the placeholder is
interpolated into the word and the entire word, if necessary, is quoted. If
the placeholder occurs within quotes, no further quoting is performed:

```javascript
sh`script --file="${'herp derp'}.txt"`; // => "script --file='herp derp.txt'"
```

This behavior can be exploited to force consistent quoting, if desired; but
both of the examples below are safe on all platforms:

```javascript
const words = ['oneword', 'two words'];
sh`minimal ${words[0]}`; // => "minimal oneword"
sh`minimal ${words[1]}`; // => "minimal 'two words'"
sh`consistent '${words[0]}'`; // => "consistent 'oneword'"
sh`consistent '${words[1]}'`; // => "consistent 'two words'"
```

##### Arrays and iterables

If a word contains a placeholder for an array (or other iterable object), then
the entire word is repeated once for each value in the array, separated by
spaces. If the array is empty, then the word is not emitted at all, and neither
is any leading whitespace.

```javascript
const files = ['foo', 'bar'];
sh`script ${files}`; // => "script foo bar"
sh`script --file=${files}`; // => "script --file=foo --file=bar"
sh`script --file=${[]}`; // => "script"
```

Note that, since special characters are their own words, the pipe operator here
is not repeated:

```javascript
const cmd = ['script', 'foo', 'bar'];
sh`${cmd}|another-script`; // => "script foo bar|another-script"
```

Multiple arrays in the same word generate a Cartesian product:

```javascript
const names = ['foo', 'bar'], exts = ['log', 'txt'];
// Same word
sh`... ${names}.${exts}`; // => "... foo.log foo.txt bar.log bar.txt"
sh`... "${names} ${exts}"`; // => "... 'foo log' 'foo txt' 'bar log' 'bar txt'"

// Not the same word (extra space just for emphasis):
sh`... ${names}   ${exts}`; // => "... foo bar   log txt"
sh`... ${names};${exts}`; // => "... foo bar;log txt"
```

Finally, if a placeholder appears in the object of a redirect operator, the
entire redirect is repeated as necessary:

```javascript
sh`script > ${['foo', 'bar']}.txt`; // => "script > foo.txt > bar.txt"
sh`script > ${[]}.txt`; // => "script"
```

##### unquoted

The `unquoted` function returns a value that will skip being quoted when used
in a placeholder, alone or in an array.

```javascript
const cmd = 'script < input.txt';
const fields = ['foo', 'bar'];
sh`${unquoted(cmd)} | json ${fields}`; // => "script < input.txt | json foo bar"
```

##### ShellString

If `ShellString.sh` is used to construct an unformatted ShellString, that value
can be used in a placeholder to insert the contents of the ShellString into the
outer template literal. This is safer than using `unquoted` as in the previous
example, but `unquoted` can be used when all you have is a string from another
(trusted!) source.

```javascript
const url = 'http://example.com/data.json?x=1&y=2';
const curl = ShellString.sh`curl -L ${url}`;
const fields = ['foo', 'bar'];
sh`${curl} | json ${fields}`; // => "curl -L 'http://example.com/data.json?x=1&y=2' | json foo bar"
```

##### Anything else

... is treated like a string—namely, a value `x` is equivalent to `'' + x`, if
not in one of the above categories.
