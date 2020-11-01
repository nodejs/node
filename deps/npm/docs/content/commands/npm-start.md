---
title: npm-start
section: 1
description: Start a package
---

### Synopsis

```bash
npm start [-- <args>]
```

### Description

This runs an arbitrary command specified in the package's `"start"` property of
its `"scripts"` object. If no `"start"` property is specified on the
`"scripts"` object, it will run `node server.js`.

As of [`npm@2.0.0`](https://blog.npmjs.org/post/98131109725/npm-2-0-0), you can
use custom arguments when executing scripts. Refer to [`npm run-script`](/commands/npm-run-script) for more details.

### See Also

* [npm run-script](/commands/npm-run-script)
* [npm scripts](/using-npm/scripts)
* [npm test](/commands/npm-test)
* [npm restart](/commands/npm-restart)
* [npm stop](/commands/npm-stop)
