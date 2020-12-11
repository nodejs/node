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

This restarts a project.  It is equivalent to running `npm run-script
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

### See Also

* [npm run-script](/commands/npm-run-script)
* [npm scripts](/using-npm/scripts)
* [npm test](/commands/npm-test)
* [npm start](/commands/npm-start)
* [npm stop](/commands/npm-stop)
* [npm restart](/commands/npm-restart)
