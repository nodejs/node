---
title: npm-set-script
section: 1
description: Set tasks in the scripts section of package.json
---

### Synopsis
An npm command that lets you create a task in the scripts section of the package.json.

```bash
npm set-script [<script>] [<command>]
```


**Example:**

* `npm set-script start "http-server ."`

```json
{
  "name": "my-project",
  "scripts": {
    "start": "http-server .",
    "test": "some existing value"
  }
}
```

### See Also

* [npm run-script](/commands/npm-run-script)
* [npm install](/commands/npm-install)
* [npm test](/commands/npm-test)
* [npm start](/commands/npm-start)
