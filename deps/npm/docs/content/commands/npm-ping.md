---
title: npm-ping
section: 1
description: Ping npm registry
---

### Synopsis

```bash
npm ping [--registry <registry>]
```

### Description

Ping the configured or given npm registry and verify authentication.
If it works it will output something like:

```bash
Ping success: {*Details about registry*}
```
otherwise you will get:
```bash
Ping error: {*Detail about error}
```

### See Also

* [npm doctor](/commands/npm-doctor)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
