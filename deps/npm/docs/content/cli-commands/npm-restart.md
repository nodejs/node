---
section: cli-commands 
title: npm-restart
description: Restart a package
---

# npm-restart(1)

## Restart a package

### Synopsis

```bash
npm restart [-- <args>]
```

### Description

This restarts a package.

This runs a package's "stop", "restart", and "start" scripts, and associated
pre- and post- scripts, in the order given below:

1. prerestart
2. prestop
3. stop
4. poststop
5. restart
6. prestart
7. start
8. poststart
9. postrestart

### Note

Note that the "restart" script is run **in addition to** the "stop"
and "start" scripts, not instead of them.

This is the behavior as of `npm` major version 2.  A change in this
behavior will be accompanied by an increase in major version number

### See Also

* [npm run-script](/cli-commands/npm-run-script)
* [npm scripts](/using-npm/scripts)
* [npm test](/cli-commands/npm-test)
* [npm start](/cli-commands/npm-start)
* [npm stop](/cli-commands/npm-stop)
* [npm restart](/cli-commands/npm-restart)