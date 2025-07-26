---
title: npm-cache
section: 1
description: Manipulates packages cache
---

### Synopsis

```bash
npm cache add <package-spec>
npm cache clean [<key>]
npm cache ls [<name>@<version>]
npm cache verify
npm cache npx ls
npm cache npx rm [<key>...]
npm cache npx info <key>...
```

Note: This command is unaware of workspaces.

### Description

Used to add, list, or clean the `npm cache` folder.
Also used to view info about entries in the `npm exec` (aka `npx`) cache folder.

#### `npm cache`

* add:
  Add the specified packages to the local cache.  This command is primarily intended to be used internally by npm, but it can provide a way to add data to the local installation cache explicitly.

* clean:
  Delete a single entry or all entries out of the cache folder.  Note that this is typically unnecessary, as npm's cache is self-healing and resistant to data corruption issues.

* ls:
  List given entries or all entries in the local cache.

* verify:
  Verify the contents of the cache folder, garbage collecting any unneeded data, and verifying the integrity of the cache index and all cached data.

#### `npm cache npx`

* ls:
  List all entries in the npx cache.

* rm:
  Remove given entries or all entries from the npx cache.

* info:
  Get detailed information about given entries in the npx cache.

### Details

npm stores cache data in an opaque directory within the configured `cache`, named `_cacache`. This directory is a [`cacache`](http://npm.im/cacache)-based content-addressable cache that stores all http request data as well as other package-related data. This directory is primarily accessed through `pacote`, the library responsible for all package fetching as of npm@5.

All data that passes through the cache is fully verified for integrity on both insertion and extraction. Cache corruption will either trigger an error, or signal to `pacote` that the data must be refetched, which it will do automatically. For this reason, it should never be necessary to clear the cache for any reason other than reclaiming disk space, thus why `clean` now requires `--force` to run.

There is currently no method exposed through npm to inspect or directly manage the contents of this cache. In order to access it, `cacache` must be used directly.

npm will not remove data by itself: the cache will grow as new packages are installed.

### A note about the cache's design

The npm cache is strictly a cache: it should not be relied upon as a persistent and reliable data store for package data. npm makes no guarantee that a previously-cached piece of data will be available later, and will automatically delete corrupted contents. The primary guarantee that the cache makes is that, if it does return data, that data will be exactly the data that was inserted.

To run an offline verification of existing cache contents, use `npm cache
verify`.

### Configuration

#### `cache`

* Default: Windows: `%LocalAppData%\npm-cache`, Posix: `~/.npm`
* Type: Path

The location of npm's cache directory.



### See Also

* [package spec](/using-npm/package-spec)
* [npm folders](/configuring-npm/folders)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [npm install](/commands/npm-install)
* [npm publish](/commands/npm-publish)
* [npm pack](/commands/npm-pack)
* [npm exec](/commands/npm-exec)
* https://npm.im/cacache
* https://npm.im/pacote
* https://npm.im/@npmcli/arborist
* https://npm.im/make-fetch-happen
