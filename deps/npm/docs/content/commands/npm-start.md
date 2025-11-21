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

This runs a predefined command specified in the `"start"` property of a package's `"scripts"` object.

If the `"scripts"` object does not define a  `"start"` property, npm will run `node server.js`.

Note that this is different from the default node behavior of running the file specified in a package's `"main"` attribute when evoking with `node .`

As of [`npm@2.0.0`](https://blog.npmjs.org/post/98131109725/npm-2-0-0), you can use custom arguments when executing scripts.
Refer to [`npm run`](/commands/npm-run) for more details.

### Example

```json
{
  "scripts": {
    "start": "node foo.js"
  }
}
```

```bash
npm start

> npm@x.x.x start
> node foo.js

(foo.js output would be here)

```

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
* [npm restart](/commands/npm-restart)
* [npm stop](/commands/npm-stop)
