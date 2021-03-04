---
title: npm-unpublish
section: 1
description: Remove a package from the registry
---

### Synopsis

To learn more about how the npm registry treats unpublish, see our <a
href="https://www.npmjs.com/policies/unpublish" target="_blank"
rel="noopener noreferrer"> unpublish policies</a>

#### Unpublishing a single version of a package

```bash
npm unpublish [<@scope>/]<pkg>@<version>
```

#### Unpublishing an entire package

```bash
npm unpublish [<@scope>/]<pkg> --force
```

### Warning

Consider using the [`deprecate`](/commands/npm-deprecate) command instead,
if your intent is to encourage users to upgrade, or if you no longer
want to maintain a package.

### Description

This removes a package version from the registry, deleting its entry and
removing the tarball.

The npm registry will return an error if you are not [logged
in](/commands/npm-login).

If you do not specify a version or if you remove all of a package's
versions then the registry will remove the root package entry entirely.

Even if you unpublish a package version, that specific name and version
combination can never be reused. In order to publish the package again,
you must use a new version number. If you unpublish the entire package,
you may not publish any new versions of that package until 24 hours have
passed.

### See Also

* [npm deprecate](/commands/npm-deprecate)
* [npm publish](/commands/npm-publish)
* [npm registry](/using-npm/registry)
* [npm adduser](/commands/npm-adduser)
* [npm owner](/commands/npm-owner)
* [npm login](/commands/npm-login)
