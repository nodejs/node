# Environment Variables

Environment variables are variables associated to the environment the Node.js process runs in.

## CLI Environment Variables

There is a set of environment variables that can be defined to customize the behavior of Node.js,
for more details refer to the [CLI Environment Variables documentation][].

## `process.env`

The basic API for interacting with environment variables is `process.env`, it consists of an object
with pre-populated user environment variables that can be modified and expanded.

For more details refer to the [`process.env` documentation][].

## DotEnv

Set of utilities for dealing with additional environment variables defined in `.env` files.

> Stability: 1.1 - Active development

<!--introduced_in=v20.12.0-->

### .env files

`.env` files (also known as dotenv files) are files that define environment variables,
which Node.js applications can then interact with (popularized by the [dotenv][] package).

The following is an example of the content of a basic `.env` file:

```text
MY_VAR_A = "my variable A"
MY_VAR_B = "my variable B"
```

This type of file is used in various different programming languages and platforms but there
is no formal specification for it, therefore Node.js defines its own specification described below.

A `.env` file is a file that contains key-value pairs, each pair is represented by a variable name
followed by the equal sign (`=`) followed by a variable value.

The name of such files is usually `.env` or it starts with `.env` (like for example `.env.dev` where
`dev` indicates a specific target environment). This is the recommended naming scheme but it is not
mandatory and dotenv files can have any arbitrary file name.

#### Variable Names

A valid variable name must contain only letters (uppercase or lowercase), digits and underscores
(`_`) and it can't begin with a digit.

More specifically a valid variable name must match the following regular expression:

```text
^[a-zA-Z_]+[a-zA-Z0-9_]*$
```

The recommended convention is to use capital letters with underscores and digits when necessary,
but any variable name respecting the above definition will work just fine.

For example, the following are some valid variable names: `MY_VAR`, `MY_VAR_1`, `my_var`, `my_var_1`,
`myVar`, `My_Var123`, while these are instead not valid: `1_VAR`, `'my-var'`, `"my var"`, `VAR_#1`.

#### Variable Values

Variable values are comprised by any arbitrary text, which can optionally be wrapped inside
single (`'`) or double (`"`) quotes.

Quoted variables can span across multiple lines, while non quoted ones are restricted to a single line.

Noting that when parsed by Node.js all values are interpreted as text, meaning that any value will
result in a JavaScript string inside Node.js. For example the following values: `0`, `true` and
`{ "hello": "world" }` will result in the literal strings `'0'`, `'true'` and `'{ "hello": "world" }'`
instead of the number zero, the boolean `true` and an object with the `hello` property respectively.

Examples of valid variables:

```text
MY_SIMPLE_VAR = a simple single line variable
MY_EQUALS_VAR = "this variable contains an = sign!"
MY_HASH_VAR = 'this variable contains a # symbol!'
MY_MULTILINE_VAR = '
this is a multiline variable containing
two separate lines\nSorry, I meant three lines'
```

#### Spacing

Leading and trailing whitespace characters around variable keys and values are ignored unless they
are enclosed within quotes.

For example:

```text
   MY_VAR_A   =    my variable a
    MY_VAR_B   =    '   my variable b   '
```

will be treaded identically to:

```text
MY_VAR_A = my variable
MY_VAR_B = '   my variable b   '
```

#### Comments

Hash-tag (`#`) characters denote the beginning of a comment, meaning that the rest of the line
will be completely ignored.

Hash-tags found within quotes are however treated as any other standard character.

For example:

```text
# This is a comment
MY_VAR = my variable # This is also a comment
MY_VAR_A = "# this is NOT a comment"
```

#### `export` prefixes

The `export` keyword can optionally be added in front of variable declarations, such keyword will be completely ignored
by all processing done on the file.

This is useful so that the file can be sourced, without modifications, in shell terminals.

Example:

```text
export MY_VAR = my variable
```

### CLI Options

`.env` files can be used to populate the `process.env` object via one the following CLI options:

* [`--env-file=file`][]

* [`--env-file-if-exists=file`][]

### Programmatic APIs

There following two functions allow you to directly interact with `.env` files:

* [`process.loadEnvFile`][] loads an `.env` file and populates `process.env` with its variables

* [`util.parseEnv`][] parses the row content of an `.env` file and returns its value in an object

[CLI Environment Variables documentation]: cli.md#environment-variables_1
[`--env-file-if-exists=file`]: cli.md#--env-file-if-existsfile
[`--env-file=file`]: cli.md#--env-filefile
[`process.env` documentation]: process.md#processenv
[`process.loadEnvFile`]: process.md#processloadenvfilepath
[`util.parseEnv`]: util.md#utilparseenvcontent
[dotenv]: https://github.com/motdotla/dotenv
