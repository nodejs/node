---
title: npm-restart
section: 1
description: Restart a package
---

### Synopsis

```bash
npm restart [-- <args>]
```

### Description

This restarts a project.  It is equivalent to running `npm run
restart`.

If the current project has a `"restart"` script specified in
`package.json`, then the following scripts will be run:

1. prerestart
2. restart
3. postrestart

If it does _not_ have a `"restart"` script specified, but it does have
`stop` and/or `start` scripts, then the following scripts will be run:

1. prerestart
2. prestop
3. stop
4. poststop
6. prestart
7. start
8. poststart
9. postrestart

### Configuration

#### `ignore-scripts`

* Default: false
* Type: Boolean

If true, npm does not run scripts specified in package.json files.

Note that commands explicitly intended to run a particular script, such as
`npm start`, `npm stop`, `npm restart`, `npm test`, and `npm run` will still
run their intended script if `ignore-scripts` is set, but they will *not*
run any pre- or post-scripts.



#### `script-shell`

* Default: '/bin/sh' on POSIX systems, 'cmd.exe' on Windows
* Type: null or String

The shell to use for scripts run with the `npm exec`, `npm run` and `npm
init <package-spec>` commands.



### See Also

* [npm run](/commands/npm-run)
* [npm scripts](/using-npm/scripts)
* [npm test](/commands/npm-test)
* [npm start](/commands/npm-start)
* [npm stop](/commands/npm-stop)
* [npm restart](/commands/npm-restart)
