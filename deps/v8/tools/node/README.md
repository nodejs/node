# Node.js Backports

We no longer maintain our own backport script.

For backporting V8 changes to Node.js, there is a useful script in
[node-core-utils][1]. You can use the `git node v8 backport` command, which will
bump the necessary V8 version numbers depending on the specific branch.

See the [Node.js documentation][2] on V8 backports for a guide.

[1]: https://github.com/nodejs/node-core-utils
[2]: https://github.com/nodejs/node/blob/master/doc/guides/maintaining-V8.md
