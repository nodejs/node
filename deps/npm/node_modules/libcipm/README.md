# Note: pending imminent deprecation

**This module will be deprecated once npm v7 is released.  Please do not rely
on it more than absolutely necessary.**

----

[`libcipm`](https://github.com/npm/libcipm) installs npm projects in a way that's
optimized for continuous integration/deployment/etc scenarios. It gives up
the ability to build its own trees or install packages individually, as well
as other user-oriented features, in exchange for speed, and being more strict
about project state.

For documentation about the associated command-line tool, see
[`cipm`](https://npm.im/cipm).

## Install

`$ npm install libcipm`

## Table of Contents

* [Features](#features)
* [API](#api)

### Features

* npm-compatible project installation
* lifecycle script support
* blazing fast
* npm-compatible caching
* errors if `package.json` and `package-lock.json` are out of sync, instead of fixing it like npm does. Essentially provides a `--frozen` install.
