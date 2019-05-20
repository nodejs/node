# About this Documentation

<!--introduced_in=v0.10.0-->
<!-- type=misc -->

Welcome to the official API reference documentation for Node.js!

Node.js is a JavaScript runtime built on the [V8 JavaScript engine][].

## Contributing

Report errors in this documentation in [the issue tracker][]. See
[the contributing guide][] for directions on how to submit pull requests.


## Stability Index

<!--type=misc-->

Throughout the documentation are indications of a section's
stability. The Node.js API is still somewhat changing, and as it
matures, certain parts are more reliable than others. Some are so
proven, and so relied upon, that they are unlikely to ever change at
all. Others are brand new and experimental, or known to be hazardous
and being redesigned.

The stability indices are as follows:

> Stability: 0 - Deprecated. The feature may emit warnings. Backward
> compatibility is not guaranteed.

<!-- separator -->

> Stability: 1 - Experimental. This feature is still under active development
> and subject to non-backward compatible changes or removal in any future
> version. Use of the feature is not recommended in production environments.
> Experimental features are not subject to the Node.js Semantic Versioning
> model.

<!-- separator -->

> Stability: 2 - Stable. Compatibility with the npm ecosystem is a high
> priority.

Use caution when making use of `Experimental` features, particularly
within modules that are dependencies (or dependencies of
dependencies) within a Node.js application. End users may not be aware that
experimental features are being used, and may experience unexpected
failures or behavior changes when API modifications occur. To help avoid such
surprises, `Experimental` features may require a command-line flag to
enable them, or may emit a process warning.
By default, such warnings are printed to [`stderr`][] and may be handled by
attaching a listener to the [`'warning'`][] event.

## JSON Output
<!-- YAML
added: v0.6.12
-->

> Stability: 1 - Experimental

Every `.html` document has a corresponding `.json` document presenting
the same information in a structured manner. This feature is
experimental, and added for the benefit of IDEs and other utilities that
wish to do programmatic things with the documentation.

## Syscalls and man pages

System calls like open(2) and read(2) define the interface between user programs
and the underlying operating system. Node.js functions
which wrap a syscall,
like [`fs.open()`][], will document that. The docs link to the corresponding man
pages (short for manual pages) which describe how the syscalls work.

Most Unix syscalls have Windows equivalents, but behavior may differ on Windows
relative to Linux and macOS. For an example of the subtle ways in which it's
sometimes impossible to replace Unix syscall semantics on Windows, see [Node.js
issue 4760](https://github.com/nodejs/node/issues/4760).

[`'warning'`]: process.html#process_event_warning
[`fs.open()`]: fs.html#fs_fs_open_path_flags_mode_callback
[`stderr`]: process.html#process_process_stderr
[the contributing guide]: https://github.com/nodejs/node/blob/master/CONTRIBUTING.md
[the issue tracker]: https://github.com/nodejs/node/issues/new
[V8 JavaScript engine]: https://v8.dev/
