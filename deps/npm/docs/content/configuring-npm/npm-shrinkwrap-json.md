---
title: npm-shrinkwrap.json
section: 5
description: A publishable lockfile
---

### Description

`npm-shrinkwrap.json` is a file created by [`npm
shrinkwrap`](/commands/npm-shrinkwrap). It is identical to
`package-lock.json`, with one major caveat: Unlike `package-lock.json`,
`npm-shrinkwrap.json` may be included when publishing a package.

The recommended use-case for `npm-shrinkwrap.json` is applications deployed
through the publishing process on the registry: for example, daemons and
command-line tools intended as global installs or `devDependencies`. It's
strongly discouraged for library authors to publish this file, since that
would prevent end users from having control over transitive dependency
updates.

If both `package-lock.json` and `npm-shrinkwrap.json` are present in a
package root, `npm-shrinkwrap.json` will be preferred over the
`package-lock.json` file.

For full details and description of the `npm-shrinkwrap.json` file format,
refer to the manual page for
[package-lock.json](/configuring-npm/package-lock-json).

### See also

* [npm shrinkwrap](/commands/npm-shrinkwrap)
* [package-lock.json](/configuring-npm/package-lock-json)
* [package.json](/configuring-npm/package-json)
* [npm install](/commands/npm-install)
