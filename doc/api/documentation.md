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

Throughout the documentation are indications of a section's stability. Some APIs
are so proven and so relied upon that they are unlikely to ever change at all.
Others are brand new and experimental, or known to be hazardous.

The stability indices are as follows:

> Stability: 0 - Deprecated. The feature may emit warnings. Backward
> compatibility is not guaranteed.

<!-- separator -->

> Stability: 1 - Experimental. The feature is not subject to Semantic Versioning
> rules. Non-backward compatible changes or removal may occur in any future
> release. Use of the feature is not recommended in production environments.

<!-- separator -->

> Stability: 2 - Stable. Compatibility with the npm ecosystem is a high
> priority.

Use caution when making use of Experimental features, particularly within
modules. End users may not be aware that experimental features are being used.
Bugs or behavior changes may surprise end users when Experimental API
modifications occur. To avoid surprises, use of an Experimental feature may need
a command-line flag. Experimental features may also emit a [warning][].

## JSON Output
<!-- YAML
added: v0.6.12
-->

Every `.html` document has a corresponding `.json` document. This is for IDEs
and other utilities that consume the documentation.

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

[`fs.open()`]: fs.html#fs_fs_open_path_flags_mode_callback
[the contributing guide]: https://github.com/nodejs/node/blob/master/CONTRIBUTING.md
[the issue tracker]: https://github.com/nodejs/node/issues/new
[V8 JavaScript engine]: https://v8.dev/
[warning]: process.html#process_event_warning
