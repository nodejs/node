---
title: npm-test
section: 1
description: Test a package
---

### Synopsis

```bash
npm test [-- <args>]

aliases: t, tst
```

### Description

This runs a predefined command specified in the `"test"` property of
a package's `"scripts"` object.

### Example

```json
{
  "scripts": {
    "test": "node test.js"
  }
}
```

```bash
npm test
> npm@x.x.x test
> node test.js

(test.js output would be here)
```



### See Also

* [npm run-script](/commands/npm-run-script)
* [npm scripts](/using-npm/scripts)
* [npm start](/commands/npm-start)
* [npm restart](/commands/npm-restart)
* [npm stop](/commands/npm-stop)
