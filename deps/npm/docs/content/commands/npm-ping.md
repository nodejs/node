---
title: npm-ping
section: 1
description: Ping npm registry
---

### Synopsis

```bash
npm ping
```

Note: This command is unaware of workspaces.

### Description

Ping the configured or given npm registry and verify authentication.
If it works it will output something like:

```bash
npm notice PING https://registry.npmjs.org/
npm notice PONG 255ms
```
otherwise you will get an error:
```bash
npm notice PING http://foo.com/
npm ERR! code E404
npm ERR! 404 Not Found - GET http://www.foo.com/-/ping?write=true
```

### Configuration

#### `registry`

* Default: "https://registry.npmjs.org/"
* Type: URL

The base URL of the npm registry.



### See Also

* [npm doctor](/commands/npm-doctor)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
