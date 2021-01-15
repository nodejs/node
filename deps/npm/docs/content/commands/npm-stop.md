---
title: npm-stop
section: 1
description: Stop a package
---

### Synopsis

```bash
npm stop [-- <args>]
```

### Description

This runs a predefined command specified in the "stop" property of a
package's "scripts" object.

Unlike with [npm start](/commands/npm-start), there is no default script
that will run if the `"stop"` property is not defined.

### Example

```json
{
  "scripts": {
    "stop": "node bar.js"
  }
}
```

```bash
npm stop

> npm@x.x.x stop
> node bar.js

(bar.js output would be here)

```

### See Also

* [npm run-script](/commands/npm-run-script)
* [npm scripts](/using-npm/scripts)
* [npm test](/commands/npm-test)
* [npm start](/commands/npm-start)
* [npm restart](/commands/npm-restart)
