[![npm](https://img.shields.io/npm/v/libcipm.svg)](https://npm.im/libcipm) [![license](https://img.shields.io/npm/l/libcipm.svg)](https://npm.im/libcipm) [![Travis](https://img.shields.io/travis/zkat/cipm.svg)](https://travis-ci.org/zkat/cipm) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/zkat/cipm?svg=true)](https://ci.appveyor.com/project/zkat/cipm) [![Coverage Status](https://coveralls.io/repos/github/zkat/cipm/badge.svg?branch=latest)](https://coveralls.io/github/zkat/cipm?branch=latest)

[`libcipm`](https://github.com/zkat/cipm) installs npm projects in a way that's
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
* [Contributing](#contributing)
* [API](#api)

### Features

* npm-compatible project installation
* lifecycle script support
* blazing fast
* npm-compatible caching
* errors if `package.json` and `package-lock.json` are out of sync, instead of fixing it like npm does. Essentially provides a `--frozen` install.

### Contributing

The libcipm team enthusiastically welcomes contributions and project
participation! There's a bunch of things you can do if you want to contribute!
The [Contributor Guide](CONTRIBUTING.md) has all the information you need for
everything from reporting bugs to contributing entire new features. Please don't
hesitate to jump in if you'd like to, or even ask us questions if something
isn't clear.
