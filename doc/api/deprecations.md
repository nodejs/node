# Deprecated APIs

Node.js may deprecate APIs when either: (a) use of the API is considered to be
unsafe, (b) an improved alternative API has been made available, or (c)
breaking changes to the API are expected in a future major release.

Node.js utilizes three kinds of Deprecations:

* Documentation-only
* Runtime
* End-of-Life

A Documentation-only deprecation is one that is expressed only within the
Node.js API docs. These generate no side-effects while running Node.js.

A Runtime deprecation will, by default, generate a process warning that will
be printed to `stderr` the first time the deprecated API is used. When the
`--throw-deprecation` command-line flag is used, a Runtime deprecation will
cause an error to be thrown.

An End-of-Life deprecation is used to identify code that either has been
removed or will soon be removed from Node.js.

## Un-deprecation

From time-to-time the deprecation of an API may be reversed. Such action may
happen in either a semver-minor or semver-major release. In such situations,
this document will be updated with information relevant to the decision.
*However, the deprecation identifier will not be modified*.

## List of Deprecated APIs

<a id="DEP0062"></a>
### DEP0062: node --debug

Type: Runtime

`--debug` activates the legacy V8 debugger interface, which has been removed as
of V8 5.8. It is replaced by Inspector which is activated with `--inspect`
instead.
