OpenSSL Style Guide
===================

Applicability
-------------

New code in OpenSSL is expected to follow the conventions in this
guide. Existing code does not uniformly comply and is being brought
up to standard gradually; non-trivial changes to existing code
should bring the affected area into compliance.

When bringing an area into compliance as part of a larger change,
do so in a separate commit -- typically one that lands first, so
that the substantive change then operates on already-compliant
code. Combining a compliance sweep with a behaviour change in one
commit makes the diff hard to review and hard to revert.

Do not bring code into compliance as part of a bug fix. Make the
minimal change that fixes the bug. This holds for any bug fix, and
especially for one that may be backported to a stable release
branch -- and at the time of the fix you often cannot know whether
it will be. Mixing compliance changes into a fix complicates
backporting and makes the change larger than it needs to be. Leave
any compliance work for a separate change.

The language is C99 (ISO/IEC 9899:1999). More modern C versions
are not yet supported on every platform OpenSSL targets and
should be avoided.

Formatting
----------

OpenSSL follows the
[WebKit coding style for C code](https://webkit.org/code-style-guidelines/).
In cases where the WebKit guide gives different rules for C and C++,
OpenSSL uses the C variant.

Whitespace, indentation, brace placement, line wrapping, alignment and
the other mechanical aspects of formatting are enforced by `clang-format`
using the [`.clang-format`](.clang-format) file at the top of this
repository. The configuration is the WebKit C style with a small set
of OpenSSL-specific customisations (notably the list of project
typedefs, the `STACK_OF` / `LHASH_OF` type macros, and the list of
statement-shaped macros).

Run `clang-format` on your changes before submitting; the output of
`clang-format` is deemed correct. See
[CONTRIBUTING.md](CONTRIBUTING.md) for the tooling (`.pre-commit-config.yaml`,
the `util/reformat-patches.sh` helper, and editor integrations).

In rare situations it may be necessary to disable `clang-format` on a
piece of code. This may be done with paired comments:

```c
/* clang-format off */
I am doing something nasty here.
Reviewers should be triggered.
/* clang-format on */
```

This should be used sparingly, and should not be used if there is any
other way to do what you are doing.

Multi-line comment blocks have an additional clang-format opt-out
via the `/**` and `/*-` markers; see [Comments](#comments).

Naming
------

### Functions and variables

A name describes what the identifier holds or what it does.
Match the name to its role: a variable holding an `X509 *` is
typically `cert`; one holding an `X509_STORE_CTX *` is typically
`ctx`; a function that counts the number of active users is
called `count_active_users()`, not `cntusr()`. Use whole words
when there is no established short form, and reuse the same
name across the codebase for the same concept rather than
inventing synonyms.

Names use lowercase with underscores (snake_case). For public
functions, snake_case applies to the portion of the name after
the uppercase subsystem prefix (see below). Do not begin a
name with an underscore; identifiers starting with an
underscore are reserved by the C standard in various contexts
and can collide with toolchain or system identifiers.

For variables, OpenSSL has well-established short forms that
are fine to use without further qualification: `ctx`, `ptr`,
`len`, `buf`, `cert`, `key`, `pkey`, `ret`, `tmp`, and similar.
Use these in preference to longer forms; do not coin a new
variant when one of these already covers the meaning. Use the
suffix `_count` for a number of items, `_len` for a byte length,
and `_size` for a size in bytes; do not invent variants like
`num_X`, `X_length`, or `X_bytes` when one of these already
applies.

A variable that mirrors notation from a standard, RFC, paper,
or other authoritative specification being implemented may use
whatever name the spec uses (for example, `n`, `e`, `d` for RSA
parameters, or `salt` and `info` for HKDF). Document the spec
citation and which variables come from it in the function or
file doxygen comment; see [Doxygen comments](#doxygen-comments)
for the form.

Outside spec-mirroring, single letters are appropriate only as
loop counters (`i`, `j`, `k`).

For functions, OpenSSL names follow a `PREFIX_[OBJECT_]action()`
shape: an uppercase subsystem prefix; then, where the function
operates on a particular object or context, that object -- usually
the uppercase or mixed-case type name; then the action, in
lowercase with underscores. Where the prefix already identifies
the object, or the function is a general subsystem utility, there
is no separate object element.
Examples: `EVP_KDF_CTX_get0_kdf` (prefix `EVP`, object `KDF_CTX`,
action `get0_kdf`), `EVP_PKEY_sign`, `OSSL_CMP_validate_msg`,
`SSL_CTX_set_verify`; and, with no object element, `BIO_eof` and
`CRYPTO_malloc`.

This shape is aspirational and describes the direction for new
code. Much of the existing API predates it and carries years of
naming baggage, so it does not uniformly conform. Do not rename
existing public functions to fit it -- that breaks the API.

Public (API) functions use the uppercase subsystem prefix.
Internal functions use the lowercase `ossl_` prefix unless they
are static (i.e., local to the source file); static functions
need no prefix.

Functions that return a pointer disclose ownership of the
returned value via a `0` or `1` suffix on the name:

- `get0_X()` returns a non-owning pointer.
- `get1_X()` returns an owning pointer; the caller is the new
  owner, of either a fresh allocation or an up-ref.

The same convention applies in reverse for setters and
pushers that take a pointer:

- `set0_X(obj, p)` and `push0_X(coll, p)` transfer ownership
  of `p` to `obj` or `coll`.
- `set1_X(obj, p)` and `push1_X(coll, p)` leave ownership
  with the caller; the callee stores a copy or up-ref.

Use these forms rather than a bare `get_` / `set_` / `push_`
whenever a pointer crosses the API boundary.

A function extended from an existing form takes an `_ex`
suffix (`_ex2` for a second extension, `_ex3` for a third,
and so on). See [Extending existing functions](#extending-existing-functions)
for when to add an extended form and how to handle the
parameter list.

### Typedefs

OpenSSL uses typedefs extensively. Struct typedefs are named in
`ALL_CAPS_WITH_UNDERSCORES`, with a subsystem prefix, and the
underlying struct tag is the lowercase form of the typedef name
suffixed `_st`:

```c
typedef struct evp_pkey_st EVP_PKEY;
```

For more examples, look in `<openssl/types.h>`.

When a typedef'd enum is used (see [Structs and typedefs](#structs-and-typedefs)
below for the policy on enums), the enum type name is lowercase
and the values are uppercase.

Function-pointer and callback typedefs use one of two
suffixes:

- `_cb` for typedefs that are user-supplied callbacks
  (`X509_STORE_CTX_verify_cb`, `pem_password_cb`).
- `_fn` for function pointers in an internal interface or
  dispatch table (`OSSL_provider_init_fn`,
  `X509_STORE_CTX_verify_fn`).

When introducing a new type, consider that a bare or generic
name may collide with system or third-party headers; OpenSSL
has historically used unprefixed names like `X509` and these
now collide with Windows headers in places. Prefix new type
names (for example `EVP_PKEY`, `OSSL_PARAM`) to avoid this.

### Macros and enum labels

Macros and labels in enums should be named in
`ALL_CAPS_WITH_UNDERSCORES`. This convention helps distinguish
macros from functions and variables.

```c
#define OPENSSL_MAGIC_FOO 0x12345
```

Error reason codes follow a `SUBSYSTEM_R_REASON` pattern,
where `_R_` is the infix marking the macro as an error reason:
`X509_R_INVALID_TRUST`, `SSL_R_NO_SHARED_CIPHER`,
`ERR_R_MALLOC_FAILURE`.

Feature-disable macros follow `OPENSSL_NO_<FEATURE>` -- for
example, `OPENSSL_NO_SOCK` (no socket support),
`OPENSSL_NO_RSA` (no RSA), `OPENSSL_NO_DEPRECATED_<MAJOR>_<MINOR>`
(no APIs deprecated as of that version). When defined, the
corresponding feature's headers and implementations are
conditionally compiled out.

Comments
--------

This section describes the form and style of code comments.
[DOCUMENTATION.md](DOCUMENTATION.md) is the companion document that
describes the policy: when a comment is required, the *trivial*
exception, and the per-field commenting requirement on structures.

Use the classic `/* ... */` comment markers. Do not use `// ...`
markers.

Comments should describe *what* the code does and *why*. Do not
parrot the effect of each statement; well-written code is its own
description of *how*. As the complexity of the code increases, the
size and detail of comments should also increase. Err in favour of
more comments rather than fewer: code that is *obvious* to you
today will not necessarily be obvious to someone else two years
later.

### Multi-line comment blocks

The preferred style for long (multi-line) comments is:

```c
/*-
 * This is the preferred style for multi-line
 * comments in the OpenSSL source code.
 * Please use it consistently.
 *
 * Description:  A column of asterisks on the left side,
 * with beginning and ending almost-blank lines.
 */
```

Both `/*-` and `/**` are recognised by the `CommentPragmas` setting
in [`.clang-format`](.clang-format) and cause the block to be left
exactly as written. Use `/*-` for plain prose comments whose layout
you want to preserve, and `/**` for doxygen blocks (see below).

### TODO and FIXME markers

Use `/* TODO: <short description> */` to mark work that should be
done later. Use `/* FIXME: <short description> */` to mark a known
incorrectness, hack, or workaround that needs to be addressed. If
a marker is worth adding, the underlying work is worth tracking:
ensure a GitHub issue is opened for it and include the issue's
full URL in the marker (e.g., `/* TODO: <short description>
(https://github.com/openssl/openssl/issues/1234) */`). Use the URL
form because OpenSSL has issue trackers in multiple repositories.

### Doxygen comments

OpenSSL code uses doxygen-style comments on functions, data
structures, and macros to make the source easier to navigate and to
translate into reference documentation. The internal-function,
struct-field, and other in-source documentation requirements set out
in [DOCUMENTATION.md](DOCUMENTATION.md) must be satisfied with
doxygen-style comments using the conventions described below.

Use the `@` form of doxygen markers (`@brief`, `@param`, `@returns`,
`@file`, `@def`, `@struct`, and so on). Do not use the `\` form
(`\brief`, `\param`, etc.).

For the full set of recognised tags and their semantics, see the
Doxygen manual: the [commands list](https://www.doxygen.nl/manual/commands.html)
is the practical reference for what you can write inside a doxygen
block; the chapter on
[documenting the code](https://www.doxygen.nl/manual/docblocks.html)
explains the block forms and where comments attach.

The following sample illustrates the convention:

```c
/**
 * @file doxysample.c
 * This is a brief file description that you may add.
 * Subsequent lines contain more detailed information about what you
 * will find defined in this file.  It is not currently required that
 * you add a file description, but it is available if you like.
 */

/**
 * @def MAX(x, y)
 * Document a macro that returns the maximum of two inputs.
 * @param x integer input value
 * @param y integer input value
 * @returns the maximum of x and y
 */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/**
 * @struct foo_st
 * @brief Description of the foo_st struct.
 * Optional more detailed description here.
 */
typedef struct foo_st {
    int a; /**< Describe the a field here */
    char b; /**< Describe the b field here */
} FOO;

/**
 * @brief Describe the function ossl_add briefly.
 * Add a more detailed description here, like sums two inputs and
 * returns the result.
 * @param a input integer to add
 * @param b input integer to add
 * @returns the sum of a and b
 */
int ossl_add(int a, int b);
```

#### Spec-mirroring variables

When a function uses variable names taken from a specification
(see [Functions and variables](#functions-and-variables) in the
Naming section), the doxygen block cites the spec and identifies
each spec-derived variable:

```c
/**
 * @brief Transmogrify Calvin into Hobbes per RFC 31337 section 1.2.3.
 *
 * Variable naming follows the spec:
 * - Calvin: input to be transmogrified
 * - Hobbes: transmogrified output (caller-allocated)
 *
 * @param Calvin pointer to the input bytes to transmogrify
 * @param Calvin_len the number of bytes available at Calvin
 * @param Hobbes pointer to the caller-allocated output buffer
 * @param Hobbes_len the number of bytes available at Hobbes
 * @returns 1 on success, 0 on failure
 * @see https://www.example.org/rfc/rfc31337.html#section-1.2.3
 * @see https://calvinandhobbes.fandom.com/wiki/Transmogrifier
 */
int ossl_transmogrify(const uint8_t *Calvin, size_t Calvin_len,
    uint8_t *Hobbes, size_t Hobbes_len);
```

#### Public functions: link the manual page

Every public function declaration in a public header must carry a
doxygen block that includes an `@see` referencing the function's
manual page in the standard `name(3)` form. This in-source comment
is a navigation aid; the canonical reference documentation lives in
the POD file under `doc/man3/` (see [DOCUMENTATION.md](DOCUMENTATION.md)).

The cross-reference is to the function name, not the POD file
name; the build emits a man-page entry per function name, so
`man X509_verify_cert` resolves regardless of which POD file
currently documents it.

```c
/**
 * @brief One-line summary of what the function does.
 * @see X509_verify_cert(3)
 */
int X509_verify_cert(X509_STORE_CTX *ctx);
```

Additional `@see` entries may be added for any manual page a caller
needs in order to use the function correctly, such as pages
documenting argument types, the flag families that affect the
function's behaviour, or closely related functions. List them
comma-separated on a single `@see`, matching the form used in POD's
`SEE ALSO` section:

```c
/**
 * @brief One-line summary of what the function does.
 * @see X509_verify_cert(3), X509_STORE_CTX_new(3),
 *      X509_VERIFY_PARAM_set_flags(3)
 */
int X509_verify_cert(X509_STORE_CTX *ctx);
```

The doxygen comment should not duplicate the POD content. Two
copies of "what this function does" inevitably diverge; the POD is
the source of truth. Keep the doxygen block to a short summary and
the `@see` references.

Structs and typedefs
--------------------

See [Typedefs](#typedefs) under Naming for naming conventions.

Typedef'd enums are used much less often than struct typedefs;
consider not using a typedef for an enum at all. A typedef'd
enum hides the integer-ness of the type from the caller, which
makes the implementation-defined underlying type easier to
forget.

Enum arguments to public functions are not permitted. C's `enum`
underlying type is implementation-defined, and adding values to
an enum can change its ABI; use `int` and document the allowed
values instead.

OpenSSL has historically made all struct definitions public, which
caused problems with maintaining binary compatibility and adding
features. New structs are opaque and expose only pointers in the
API; the struct definition is placed in a local header file that
is not exported. Legacy structs that are still part of the public
ABI are exempt; do not add new public struct definitions.

In practice, the opaque pattern is to forward-declare the typedef
in the public header (`typedef struct foo_st FOO;`, with no struct
body) and place the `struct foo_st { ... };` definition in a local
header that is not exported. Callers see only the pointer type.

Bitfield layout is implementation-defined and varies across
compilers and ABIs. Where that layout is observable -- in structs
that are part of the public ABI or that mirror a wire or file
format -- avoid bitfields and use explicit shifts and masks on a
regular integer instead.

Flexible array members (C99 trailing `[]`) are permitted and
preferred over the older `[1]` "struct hack" for variable-length
trailing data. Remember that `sizeof(struct)` does not include the
flexible member; allocate the trailing data explicitly when the
struct is created.

C99 designated initializers (`{ .field = value }`) are encouraged
for struct initialisation, particularly where they make the field
assignments self-documenting.

A trailing comma in an initializer list is a layout hint to
`clang-format`: with it the list is kept one element per line;
without it the formatter may pack the list onto fewer lines.
Most of the time you do not want a trailing comma; omit it
unless you specifically want to lock the one-per-line layout
(for example, in a multi-row table of values).

Integers
--------

Prefer explicitly-sized integers over generic C ones where the
size matters. To represent a byte use `uint8_t`, not
`unsigned char`; for a two-byte field, `uint16_t` rather than
`unsigned short`.

Avoid `long` and `long long` specifically. `long` is 32 bits on
64-bit Windows and 64 bits on 64-bit Linux; using it for "at
least 32 bits" produces code that works inconsistently across
platforms. Use `int32_t`, `int64_t`, `size_t`, or another
`<stdint.h>` type as appropriate.

Sizes are `size_t`. When converting to or from `int` for legacy
reasons, check for overflow and underflow.

Add an integer literal suffix when the literal participates in a
shift or appears in an expression involving a wider type --
without a suffix the literal is `int`. Use `U` for unsigned
semantics (`1U << 31`) and the `UINT8_C` through `UINT64_C`
macros from `<stdint.h>` for explicit widths (`UINT32_C(1) << 31`,
`UINT64_C(1) << 63`). Avoid `UL` and `ULL`, for the same reason
as `long` / `long long`: their widths vary by platform.

Bit shifts should be performed on unsigned operands.
Left-shifting a signed value is undefined behaviour when the
operand is negative or when the result reaches the sign bit;
right-shifting a signed negative value is implementation-defined.
Combined with the literal-suffix rule above, shifts of constants
typically take the form `UINT32_C(1) << n` or `(uint32_t)x << n`.

In structs that are retained across the lifetime of a connection,
new integer fields whose value range is known should use a smaller
integer type (`uint8_t`, `uint16_t`) where doing so is
straightforward. This reduces per-connection memory in server
processes. Do not make code significantly more complex to achieve
it, and continue to bounds-check at the struct boundary.

This narrowing should not propagate to local variables or function
parameters; those use the conventional integer types so callers
are not forced to deal with narrow types.

Do not retroactively narrow existing integer fields in legacy
structs; this risks ABI breakage.

When doing arithmetic, account for overflow.

Use `int` with `0` / `1` for boolean values, both in public API
and internal code. Do not introduce `<stdbool.h>` for new code;
the public API convention is `int`, and using `bool` internally
just to convert to `int` at the API boundary adds friction
without enough benefit.

Except in platform-specific code, do not use `ssize_t`; MSVC lacks
it. Use `size_t` and signal errors out-of-band (see
[Return values in new code](#return-values-in-new-code)).

Preprocessor directives
-----------------------

Headers use traditional include guards in the `#if defined()`
form rather than `#pragma once`, which is non-standard:

```c
#if !defined(OPENSSL_FOO_H)
#define OPENSSL_FOO_H

/* ... header contents ... */

#endif /* defined(OPENSSL_FOO_H) */
```

Prefer `#if defined(FOO)` and `#if !defined(FOO)` to `#ifdef` and
`#ifndef`. This allows logical operations when conditional
compilation is dependent on more than one variable, without
nesting multiple blocks.

All `#endif` blocks must have a comment matching their `#if`:

```c
#if defined(OPENSSL_LINUX) && (!defined(OPENSSL_NO_HOOBLA) || !defined(OPENSSL_BULA))
...
#endif /* defined(OPENSSL_LINUX) && (!defined(OPENSSL_NO_HOOBLA) || !defined(OPENSSL_BULA)) */
```

Minimise the footprint of conditional compilation in source
code: the more conditional code is concentrated and confined,
the easier the unconditional flow is to read.

Concentrate conditional compilation rather than dispersing it.
Do not duplicate the same OS-dispatch ladder across the
codebase:

```c
#if defined(OPENSSL_OS_FOO) || defined(OPENSSL_OS_BAR)
    stuff the way foo or bar does it;
#elif defined(OPENSSL_OS_BLAH) || defined(OPENSSL_OS_WOOF)
    stuff the way blah or woof does it;
#endif /* defined(OPENSSL_OS_FOO) || defined(OPENSSL_OS_BAR) */
```

For OS-dependent code in particular, put the directives inside
a single function that wraps the OS-dependent work, so callers
see a clean interface. When the OS-dependent implementations
are large, put them in separate files (`stuff_foo.c`,
`stuff_blah.c`) implementing a common function and select the
appropriate file via the build process; this lets non-mainstream
platforms add an implementation file without patching shared
code.

When a feature can be compiled out, prefer to provide a no-op
stub implementation of its functions in the disabled case
rather than wrapping every call site in `#if`. Callers then
invoke the functions unconditionally and the compiler discards
the stubs:

```c
#if defined(OPENSSL_NO_FOO)
static ossl_inline int ossl_foo_init(void) { return 1; }
static ossl_inline void ossl_foo_cleanup(void) { }
#else
int ossl_foo_init(void);
void ossl_foo_cleanup(void);
#endif /* defined(OPENSSL_NO_FOO) */
```

Macros and enums
----------------

**Just use a function, not a macro.** OpenSSL has historically
used macros heavily to avoid function-call overhead, but modern
compilers inline well; the trade-offs that justified that pattern
no longer apply. Where a macro is genuinely unavoidable, the
rules below apply.

For the naming convention used for macros and enum labels, see the
[Macros and enum labels](#macros-and-enum-labels) subsection of
Naming above.

Enums are preferred when defining several related constants.
Enum arguments to public functions are not permitted, because
C's `enum` underlying type is implementation-defined and adding
values can change ABI; see
[Structs and typedefs](#structs-and-typedefs) for the rule and
the canonical alternative (use `int` and document the allowed
values).

Where the constants need a fixed underlying width (for ABI or
wire-format reasons), use `#define` or `static const` with an
explicit-width type from `<stdint.h>` instead, since enum width
is implementation-defined.

### Avoid complex macros

Avoid complex or clever macros: they are hard to read, debug, and
maintain. Do not nest macros calling other macros.

### Avoid function-like macros

Prefer functions over function-like macros. Do not optimise for
function-call overhead without first measuring with a function
implementation; if the function is hot enough to need inlining,
mark it `ossl_inline` rather than converting it to a macro.

### Macro parenthesisation

Always parenthesise arguments in function-like macros to prevent
operator-precedence issues during expansion. Enclose the entire
macro definition in parentheses if it expands to an expression, so
the expansion evaluates correctly inside larger expressions. For
example:

```c
#define BOB(blah) ((blah) + 42 - 23)
```

### Multi-statement macros

Enclose multi-statement macros in a `do { } while (0)` block. Do
not include a semicolon at the end, and do not use bare braces
(which fail when followed by `else`). For example:

```c
/* This is bad. */
#define KERMIT(x) muppet((x)); frog((x)); green((x))
if (something)
    KERMIT(bob);
else /* This now breaks. */

/* This is also bad, because now you have to omit the semicolon. */
#define KERMIT(x) { muppet((x)); frog((x)); green((x)) }
if (something)
    KERMIT(bob)  /* No semicolon. */
else

/* This works. */
#define KERMIT(x) do { muppet((x)); frog((x)); green((x)) } while (0)
if (something)
    KERMIT(bob);
else

/*
 * But just use a function -- now we know that x is an integer that
 * has something to do with frogginess and we gain some type safety.
 */
static void kermit(int frogginess)
{
    muppet(frogginess);
    frog(frogginess);
    green(frogginess);
}
if (something)
    kermit(bob);
else
```

### Do not include files as multi-line macros

Do not put code in a file and include it inline:

```c
    ...
    printf("Yolo\n");
#include "./abagfullofcode.inc"
    printf("That was fun\n");
    ...
```

Either make a function out of the code and call it, or put the code
in place.

### Be careful with macro arguments that have side effects

Be careful when writing a function-like macro that could be called
with arguments that have side effects. Because a macro may expand an
argument more than once, a side-effecting argument (`n++`, a function
call, a volatile access) can then be evaluated more than once, with
unexpected results:

```c
#define SQUARE(x) ((x) * (x))

int n = 1;
int result = SQUARE(n++); /* expands to ((n++) * (n++)) -- evaluates twice */
```

Where it can reasonably be avoided, prefer a form that expands each
argument exactly once -- a function, or an `ossl_inline` function
for a fixed type. If there is any doubt that your function-like
macro could be called with arguments that have side effects, treat
that as a sign to follow the advice in
[Avoid function-like macros](#avoid-function-like-macros) and make
it a real function. Some macros cannot avoid it: a type-generic macro
such as `MAX` must name each operand and so evaluates it more than
once. When that is unavoidable, say so at the definition and avoid
passing side-effecting expressions at the call site.

### Avoid macros that depend on magic names

Do not write macros that rely on a particular variable name being
in scope at the call site:

```c
#define FOO(val) bar(index, (val)) /* requires `index' to exist */
```

This is confusing to the reader and prone to breakage from
seemingly innocent changes.

### Avoid macros that expand to l-values

Do not write a macro that expands to something assignable:

```c
#define FIELD(p) (((struct foo *)(p))->field)

FIELD(x) = y; /* legal C, but the macro hides the assignment */
```

Use an accessor function or expose the field directly through a
typed pointer.

### Avoid macros that affect control flow

Do not write macros that `return`, `goto`, `break`, or `continue`
out of their expansion. Such macros hide control flow from a
reader at the call site, who sees what looks like a function
call but which may exit the surrounding function or jump out of
a loop:

```c
#define RETURN_IF_NULL(p) do { if ((p) == NULL) return -1; } while (0)

int ossl_frobnicate(void *p)
{
    RETURN_IF_NULL(p); /* may return from ossl_frobnicate() -- not visible at the call site */
    /* ... */
}
```

### Avoid `#` and `##` in new code

The stringification (`#`) and token-pasting (`##`) operators are
forbidden in new code. Existing macros that use them (notably the
`DECLARE_*` and `IMPLEMENT_*` macro families) are not retroactively
changed; new code should achieve the same effect through
functions.

### Use variadic macros sparingly

Variadic macros (`__VA_ARGS__`) are permitted but should be used
sparingly: prefer a function or a small set of helper functions
where possible. They are harder to reason about and debug than
functions, and the rules around zero variadic arguments and
`__VA_ARGS__` forwarding are subtle.

Functions
---------

A function should do one thing and be short enough that a
reader can hold its behaviour in their head while reading it.
Length follows from complexity, not the other way around: a
long but flat function (for example, a single switch dispatching
to many cases) is fine; a short function with three levels of
nested control flow is not.

When complexity grows, factor out helpers with descriptive
names. A large number of local variables is a signal that this
factoring is overdue; consider splitting before reaching for a
comment to explain the variables. Performance-critical helpers
can be marked `inline`; see
[Avoid function-like macros](#avoid-function-like-macros) for
why this is preferable to a macro.

In function prototypes, include parameter names alongside their
types. C does not require this, but it carries useful information
for the reader; the name in the prototype should match the name
in the definition.

### Functions with no arguments

A function that takes no arguments must declare so explicitly
with `void` in its parameter list: `int f(void);`, not
`int f();`. The latter declares the parameter list as
unspecified and prevents the compiler from checking calls.

### Internal linkage

Functions that are local to a single source file are declared
`static`. Static functions need no `ossl_` prefix (see
[Naming](#functions-and-variables) above) and do not appear in
the symbol table of the resulting object file.

### Parameter ordering

In OpenSSL's API style, a context parameter (an `SSL_CTX *`,
`EVP_PKEY_CTX *`, `OSSL_LIB_CTX *`, or similar) is the first
parameter. The order of the remaining parameters is at the
function's discretion but should be consistent with similar
functions in the same subsystem.

### `const`-correctness

Pointer parameters that are not modified by the function should
be declared `const`; likewise, pointer return values that the
caller must not modify should be declared `const`. The
return-side rule pairs with the `get0_X()` ownership convention:
a non-owning pointer is typically a read-only view, while an
owning pointer returned by `get1_X()` is non-`const` because the
caller controls it. The `const` qualifier documents the contract,
allows callers to pass or receive `const`-qualified data without
casts, and lets the compiler catch accidental modification.

### Return values in legacy code

Historically, functions in OpenSSL can return values of many different
kinds, and one of the most common is a value indicating whether the
function succeeded or failed. Usually this is:

- `1`: success
- `0`: failure

Other patterns appear in legacy code:

- `-1` indicates a serious error (internal error or memory
  allocation failure), and in some subsystems (BIO, SSL, etc.)
  means "should retry"
- `>= 1` indicates success with the value carrying additional
  information; `<= 0` indicates failure with the value indicating
  the reason

Functions that return a computed value (not a success/failure
indicator) are exempt.

**Read the existing return-value contract carefully before
modifying legacy code.** OpenSSL's legacy return-value
conventions are not uniform -- a function may use values,
overloadings, or semantics outside the patterns above -- and
bugs have been introduced into OpenSSL when contributors
assumed a function followed a familiar pattern when it did not.
The contract is part of the API, not just a stylistic choice.

### Return values in new code

For new code, functions should return `int` with `1` on success
and `0` on error. Do not overload the return value to both
signal success/failure and output an integer. For example:

```c
/**
 * @brief ossl_snuffle_thingamabob snuffles a thingamabob from bytes of input.
 * If a valid thingamabob is snuffled, the result is stored in
 * *out_thingamabob. On failure a snuffling error code is stored
 * in *out_err.
 * @param input pointer to the bytes to snuffle
 * @param input_len the number of bytes available to snuffle from input
 * @param out_err pointer to an integer to store an error code
 * @param out_thingamabob pointer to a thingamabob to store the output
 * @returns 1 if a thingamabob was snuffled and stored, 0 otherwise.
 */
int ossl_snuffle_thingamabob(const uint8_t *input, size_t input_len,
    int *out_err, thingamabob *out_thingamabob);
```

If a function outputs a single pointer and no other values,
return the pointer directly, with `NULL` on error.

### Checking function arguments

A public function must verify that its arguments are sensible
and return its documented failure value if they are not.
Typical checks include:

- non-optional pointer arguments are not NULL;
- numeric arguments are within their expected ranges.

Public-API callers are outside the OpenSSL development envelope.
The contract cannot be enforced through code review, so a NULL
non-optional pointer or an out-of-range integer is a possibility
that must be handled defensively at the boundary. Failing with a
documented error code on the error stack is preferable to a
SIGSEGV in the calling application's process.

For NULL pointer arguments, the canonical pattern is:

```c
if (arg == NULL) {
    ERR_raise(ERR_LIB_<lib>, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
}
```

Use the function's documented failure value in place of `0`
where it differs (`NULL` for pointer-returning functions, `-1`
for functions that may return `-1`, and so on).

Internal functions must not repeat these checks. Their callers
are us; the contract is enforceable in code review, and a NULL
or out-of-range argument is a programmer error of the same
character as the impossibilities discussed under
[Assertions](#assertions). A runtime check at an internal call
site is dead on any correct execution, and the untested branch
is itself attack surface. Use `assert()` instead where you want
to document an internal invariant.

### Extending existing functions

When an existing public function needs additional parameters,
keep the original and add a new function with the same name plus
an `_ex` suffix (`RAND_bytes_ex` extends `RAND_bytes`). Further
extensions use `_ex2`, `_ex3`, and so on.

The extended function preserves the existing parameters in their
existing order. New parameters may be inserted at any position
(they do not have to be at the end); parameters that are no
longer needed may be removed.

### Centralised exiting of functions

When a function exits from multiple locations and some common
work (such as cleanup) has to be done at every exit, use `goto`
to a single exit label. Return directly when there is no cleanup
to do. The rationale:

- a single exit point is easier to read and follow;
- it reduces excessive control structures and nesting;
- it avoids errors caused by failing to update multiple exit
  points when the code changes;
- it lets the compiler avoid emitting redundant cleanup code.

For example:

```c
int ossl_do_thing(const uint8_t *in, size_t in_len)
{
    int ret = 0;
    uint8_t *buf = OPENSSL_malloc(in_len);

    if (buf == NULL)
        return 0;

    if (!ossl_step1(in, in_len, buf))
        goto out;
    if (!ossl_step2(buf, in_len))
        goto out;

    ret = 1;
out:
    OPENSSL_free(buf);
    return ret;
}
```

Error reporting
---------------

OpenSSL surfaces errors through a per-thread error stack; see
`ERR_raise(3)` for the calls and `include/openssl/err.h` for the
available reason codes. This section describes the conventions
for using them.

Raise at the leaf. The function that detects the failure pushes
the error; intermediate wrappers that propagate the failure
value must not re-raise. Re-raising on each frame floods the
stack with duplicates and obscures the originating condition.

Use the `ERR_LIB_<subsystem>` corresponding to the function's
home directory (`ERR_LIB_X509` in code under `crypto/x509/`, and
so on). Use a cross-library reason (`ERR_R_PASSED_NULL_PARAMETER`,
`ERR_R_MALLOC_FAILURE`, `ERR_R_INTERNAL_ERROR`, and others) for
portable failure modes; use a `SUBSYSTEM_R_REASON`
(`X509_R_INVALID_TRUST`, etc.) for domain-specific ones.

Do not call `ERR_clear_error` at function entry; the error stack
belongs to the caller, who may have pushed errors before
invoking you that they intend to inspect.

Use `ERR_set_mark` / `ERR_pop_to_mark` to suppress error-stack
pollution from operations expected to fail sometimes (a
speculative parse, a capability probe), leaving earlier errors
intact.

Allocating memory
-----------------

Use the `OPENSSL_malloc` family for general allocation; see
`OPENSSL_malloc(3)` for the full set of calls. Do not mix these
with the C standard library's `malloc()` / `free()` family;
allocations made with one set must be released with the matching
set, and OpenSSL can be built with custom allocator hooks that
the C library does not know about.

For arrays, use `OPENSSL_malloc_array()` and
`OPENSSL_realloc_array()`, which take the element size and
element count separately and check for integer overflow.

Memory holding sensitive material (key bytes, plaintext,
internal state of cryptographic primitives) must be cleansed
before release. `OPENSSL_clear_free()` combines cleansing and
freeing; `OPENSSL_cleanse()` wipes without freeing. For
long-lived sensitive data, use the `OPENSSL_secure_malloc()`
family (`OPENSSL_secure_malloc(3)`), which allocates from a
separate non-pageable secure heap, and release with
`OPENSSL_secure_clear_free()`.

An API that owns internal state requires both an initialisation
function to set it up and a completion function to release it.
This is the standard constructor/destructor pair for opaque
types; see [Structs and typedefs](#structs-and-typedefs).

Processor-specific code
-----------------------

The only reason for processor-specific code in OpenSSL is
performance. Every processor-specific path must have a
platform-neutral pure-C implementation as a fallback, because
not every target architecture or build configuration enables
the processor-specific path. OpenSSL selects between
implementations at runtime via the CPU-capability detection in
`OPENSSL_cpuid_setup` and the `OPENSSL_*` capability flags;
processor-specific code must integrate with this dispatch.

Cryptographic primitives operating on secret data must execute
in time independent of those secrets. Avoid secret-dependent
branches, secret-indexed memory accesses, and variable-time
arithmetic (such as variable-time multiplication or division)
on words derived from secrets. Hand-coded asm is sometimes used
specifically to force a particular sequence of constant-time
operations that a compiler might otherwise rewrite.

Short processor-specific operations are typically written as
inline assembly. Use a `static inline` function when the asm
constraints permit it. When the asm requires a compile-time
constant operand (an `i` constraint), use a statement-expression
macro instead, because a function parameter does not satisfy the
immediate-constant constraint. When `asm()` has side effects the
compiler cannot see, mark it `volatile`; do not mark `volatile`
unnecessarily as that limits optimisation.

When writing a single inline assembly statement containing
multiple instructions, put each instruction on a separate line
in a separate quoted string, and end each string except the
last with `\n\t` to properly indent the next instruction in the
assembly output:

```c
asm("magic %reg1, #42\n\t"
    "more_magic %reg2, %reg3"
    : /* outputs */ : /* inputs */ : /* clobbers */);
```

Large, non-trivial assembly functions go in pure assembly
modules, with corresponding C prototypes. The preferred way to
generate these is *perlasm*: a Perl script that generates a
`.s` file. Perlasm allows symbolic names for variables
(registers and stack-allocated locals) that are independent of
the specific assembler, and supports multiple ABIs and
assemblers from a single source by adhering to its coding
rules. See `crypto/perlasm/x86_64-xlate.pl` for an example.

Compiler intrinsics are permitted but used sparingly. They are
appropriate for self-contained SIMD acceleration where the
intrinsic vocabulary is well-supported across our target
compilers and the code does not need to span multiple ABIs --
`crypto/evp/enc_b64_avx2.c` (AVX2 base64) is an example.
Intrinsics are not appropriate for cryptographic primitives
where constant-time execution is required (the compiler may
reorder, branch, or otherwise alter the timing), or where an
existing perlasm implementation already covers the multi-ABI
case.

Assertions
----------

Assertions check programmer errors -- invariants, preconditions, and
postconditions that must hold in any correctly-functioning build.
They are not for runtime conditions such as allocation failure, I/O
errors, or malformed input from callers; those are errors the
surrounding code must handle and propagate.

OpenSSL provides three assertion forms, which differ in their
behaviour depending on whether `NDEBUG` is defined (release) or not
(debug):

| Form | Failure (debug) | Failure (release) | Success |
|---|---|---|---|
| `assert(e)` | abort | `e` not evaluated | no effect |
| `ossl_assert(e)` | abort | returns 0 | returns 1 |
| `OPENSSL_assert(e)` | abort | abort | no effect |

Choosing between these forms is a trade-off, not a default. Each
form pays a cost somewhere:

- `OPENSSL_assert()` terminates the host process when an invariant
  fails, which is hostile to applications that link against
  OpenSSL.
- `ossl_assert()` returns failure in release builds so the host
  process survives, but the caller must then handle a failure for
  a condition that, by definition, cannot occur in correct code.
  That handling code is dead on any correct execution and cannot
  be exercised by ordinary tests; untested branches accumulate
  their own bugs and become part of the attack surface.
- `assert()` is silently dropped in release builds, so an invariant
  violation in production passes through to downstream code that
  may then operate on inconsistent state.

Use `ossl_assert()` when the surrounding function already returns
success/failure and the recovery path collapses naturally into the
function's existing error path: push an internal error and return
the function's failure value. The recovery code is then colocated
with tested error handling and is not a structurally new branch:

```c
if (!ossl_assert(invariant_holds)) {
    ERR_raise(ERR_LIB_..., ERR_R_INTERNAL_ERROR);
    return 0;
}
```

Use `assert()` for impossible cases in internal code -- typically
`switch` defaults, unreachable branches in helpers, and invariants
local to a function whose contract makes the violation strictly
impossible. The release-build behaviour ("do nothing") is the
right choice here, because the alternative is untested recovery
code for a case that cannot occur, and that code is itself a
hazard. The assertion expression must be free of side effects,
because `assert()` does not evaluate it in release builds.

Use `OPENSSL_assert()` only when continued execution would be more
dangerous than termination -- typically when global library state
is irrecoverably corrupted -- or in applications, test programs,
and fuzzers where termination on a failed check is desired.
`OPENSSL_assert()` aborts in all builds, including production.
