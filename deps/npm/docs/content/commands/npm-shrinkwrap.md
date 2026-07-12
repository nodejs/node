---
title: npm-shrinkwrap
section: 1
description: Lock down dependency versions for publication
---

### Synopsis

```bash
npm shrinkwrap
```

Note: This command is unaware of workspaces.

### Description

This command repurposes `package-lock.json` into a publishable `npm-shrinkwrap.json` or simply creates a new one.
The file created and updated by this command will then take precedence over any other existing or future `package-lock.json` files.
For a detailed explanation of the design and purpose of package locks in npm, see [package-lock-json](/configuring-npm/package-lock-json).

### See Also

* [npm install](/commands/npm-install)
* [npm run](/commands/npm-run)
* [npm scripts](/using-npm/scripts)
* [package.json](/configuring-npm/package-json)
* [package-lock.json](/configuring-npm/package-lock-json)
* [npm-shrinkwrap.json](/configuring-npm/npm-shrinkwrap-json)
* [npm ls](/commands/npm-ls)
