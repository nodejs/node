# About this Documentation

<!-- type=misc -->

The goal of this documentation is to comprehensively explain the Node.js
API, both from a reference as well as a conceptual point of view.  Each
section describes a built-in module or high-level concept.

Where appropriate, property types, method arguments, and the arguments
provided to event handlers are detailed in a list underneath the topic
heading.

Every `.html` document has a corresponding `.json` document presenting
the same information in a structured manner.  This feature is
experimental, and added for the benefit of IDEs and other utilities that
wish to do programmatic things with the documentation.

Every `.html` and `.json` file is generated based on the corresponding
`.md` file in the `doc/api/` folder in Node.js's source tree.  The
documentation is generated using the `tools/doc/generate.js` program.
The HTML template is located at `doc/template.html`.


If you find an error in this documentation, please [submit an issue][]
or see [the contributing guide][] for directions on how to submit a patch.

## Stability Index

<!--type=misc-->

Throughout the documentation, you will see indications of a section's
stability.  The Node.js API is still somewhat changing, and as it
matures, certain parts are more reliable than others.  Some are so
proven, and so relied upon, that they are unlikely to ever change at
all.  Others are brand new and experimental, or known to be hazardous
and in the process of being redesigned.

The stability indices are as follows:

```txt
Stability: 0 - Deprecated
This feature is known to be problematic, and changes are
planned.  Do not rely on it.  Use of the feature may cause warnings.  Backwards
compatibility should not be expected.
```

```txt
Stability: 1 - Experimental
This feature is subject to change, and is gated by a command line flag.
It may change or be removed in future versions.
```

```txt
Stability: 2 - Stable
The API has proven satisfactory. Compatibility with the npm ecosystem
is a high priority, and will not be broken unless absolutely necessary.
```

```txt
Stability: 3 - Locked
Only bug fixes, security fixes, and performance improvements will be accepted.
Please do not suggest API changes in this area; they will be refused.
```

## JSON Output

> Stability: 1 - Experimental

Every HTML file in the markdown has a corresponding JSON file with the
same data.

This feature was added in Node.js v0.6.12.  It is experimental.

## Syscalls and man pages

System calls like open(2) and read(2) define the interface between user programs
and the underlying operating system. Node functions which simply wrap a syscall,
like `fs.open()`, will document that. The docs link to the corresponding man
pages (short for manual pages) which describe how the syscalls work.

**Caveat:** some syscalls, like lchown(2), are BSD-specific. That means, for
example, that `fs.lchown()` only works on Mac OS X and other BSD-derived systems,
and is not available on Linux.

Most Unix syscalls have Windows equivalents, but behavior may differ on Windows
relative to Linux and OS X. For an example of the subtle ways in which it's
sometimes impossible to replace Unix syscall semantics on Windows, see [Node
issue 4760](https://github.com/nodejs/node/issues/4760).

[submit an issue]: https://github.com/nodejs/node/issues/new
[the contributing guide]: https://github.com/nodejs/node/blob/master/CONTRIBUTING.md
