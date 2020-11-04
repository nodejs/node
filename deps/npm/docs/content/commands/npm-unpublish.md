---
title: npm-unpublish
section: 1
description: Remove a package from the registry
---

### Synopsis

#### Unpublishing a single version of a package

```bash
npm unpublish [<@scope>/]<pkg>@<version>
```

#### Unpublishing an entire package

```bash
npm unpublish [<@scope>/]<pkg> --force
```

### Warning

Consider using the `deprecate` command instead, if your intent is to encourage users to upgrade, or if you no longer want to maintain a package.

### Description

This removes a package version from the registry, deleting its
entry and removing the tarball.

If no version is specified, or if all versions are removed then
the root package entry is removed from the registry entirely.

Even if a package version is unpublished, that specific name and
version combination can never be reused. In order to publish the
package again, a new version number must be used. If you unpublish the entire package, you may not publish any new versions of that package until 24 hours have passed.

To learn more about how unpublish is treated on the npm registry, see our <a href="https://www.npmjs.com/policies/unpublish" target="_blank" rel="noopener noreferrer"> unpublish policies</a>. 


### See Also

* [npm deprecate](/commands/npm-deprecate)
* [npm publish](/commands/npm-publish)
* [npm registry](/using-npm/registry)
* [npm adduser](/commands/npm-adduser)
* [npm owner](/commands/npm-owner)
