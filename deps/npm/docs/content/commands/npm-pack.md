---
title: npm-pack
section: 1
description: Create a tarball from a package
---

### Synopsis

```bash
npm pack [[<@scope>/]<pkg>...] [--dry-run]
```

### Description

For anything that's installable (that is, a package folder, tarball,
tarball url, git url, name@tag, name@version, name, or scoped name), this
command will fetch it to the cache, copy the tarball to the current working
directory as `<name>-<version>.tgz`, and then write the filenames out to
stdout.

If the same package is specified multiple times, then the file will be
overwritten the second time.

If no arguments are supplied, then npm packs the current package folder.

The `--dry-run` argument will do everything that pack usually does without
actually packing anything.  That is, it reports on what would have gone
into the tarball, but nothing else.

### See Also

* [npm-packlist package](http://npm.im/npm-packlist)
* [npm cache](/commands/npm-cache)
* [npm publish](/commands/npm-publish)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
