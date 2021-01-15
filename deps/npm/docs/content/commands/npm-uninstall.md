---
title: npm-uninstall
section: 1
description: Remove a package
---

### Synopsis

```bash
npm uninstall [<@scope>/]<pkg>[@<version>]... [-S|--save|--no-save]

aliases: remove, rm, r, un, unlink
```

### Description

This uninstalls a package, completely removing everything npm installed
on its behalf.

It also removes the package from the `dependencies`, `devDependencies`,
`optionalDependencies`, and `peerDependencies` objects in your
`package.json`.

Futher, if you have an `npm-shrinkwrap.json` or `package-lock.json`, npm
will update those files as well.

`--no-save` will tell npm not to remove the package from your
`package.json`, `npm-shrinkwrap.json`, or `package-lock.json` files.

`--save` or `-S` will tell npm to remove the package from your
`package.json`, `npm-shrinkwrap.json`, and `package-lock.json` files.
This is the default, but you may need to use this if you have for
instance `save=false` in your `npmrc` file

In global mode (ie, with `-g` or `--global` appended to the command),
it uninstalls the current package context as a global package.
`--no-save` is ignored in this case.

Scope is optional and follows the usual rules for [`scope`](/using-npm/scope).

### Examples

```bash
npm uninstall sax
```

`sax` will no longer be in your `package.json`, `npm-shrinkwrap.json`, or
`package-lock.json` files.

```bash
npm uninstall lodash --no-save
```

`lodash` will not be removed from your `package.json`,
`npm-shrinkwrap.json`, or `package-lock.json` files.

### See Also

* [npm prune](/commands/npm-prune)
* [npm install](/commands/npm-install)
* [npm folders](/configuring-npm/folders)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
