---
title: npm-publish
section: 1
description: Publish a package
---

### Synopsis

```bash
npm publish [<tarball>|<folder>] [--tag <tag>] [--access <public|restricted>] [--otp otpcode] [--dry-run]

Publishes '.' if no argument supplied
Sets tag 'latest' if no --tag specified
```

### Description

Publishes a package to the registry so that it can be installed by name.

By default npm will publish to the public registry. This can be overridden
by specifying a different default registry or using a
[`scope`](/using-npm/scope) in the name (see
[`package.json`](/configuring-npm/package-json)).

* `<folder>`: A folder containing a package.json file

* `<tarball>`: A url or file path to a gzipped tar archive containing a
  single folder with a package.json file inside.

* `[--tag <tag>]`: Registers the published package with the given tag, such
  that `npm install <name>@<tag>` will install this version.  By default,
  `npm publish` updates and `npm install` installs the `latest` tag. See
  [`npm-dist-tag`](npm-dist-tag) for details about tags.

* `[--access <public|restricted>]`: Tells the registry whether this package
  should be published as public or restricted. Only applies to scoped
  packages, which default to `restricted`.  If you don't have a paid
  account, you must publish with `--access public` to publish scoped
  packages.

* `[--otp <otpcode>]`: If you have two-factor authentication enabled in
  `auth-and-writes` mode then you can provide a code from your
  authenticator with this. If you don't include this and you're running
  from a TTY then you'll be prompted.

* `[--dry-run]`: As of `npm@6`, does everything publish would do except
  actually publishing to the registry. Reports the details of what would
  have been published.

The publish will fail if the package name and version combination already
exists in the specified registry.

Once a package is published with a given name and version, that specific
name and version combination can never be used again, even if it is removed
with [`npm unpublish`](/commands/npm-unpublish).

As of `npm@5`, both a sha1sum and an integrity field with a sha512sum of the
tarball will be submitted to the registry during publication. Subsequent
installs will use the strongest supported algorithm to verify downloads.

Similar to `--dry-run` see [`npm pack`](/commands/npm-pack), which figures
out the files to be included and packs them into a tarball to be uploaded
to the registry.

### Files included in package

To see what will be included in your package, run `npx npm-packlist`.  All
files are included by default, with the following exceptions:

- Certain files that are relevant to package installation and distribution
  are always included.  For example, `package.json`, `README.md`,
  `LICENSE`, and so on.

- If there is a "files" list in
  [`package.json`](/configuring-npm/package-json), then only the files
  specified will be included.  (If directories are specified, then they
  will be walked recursively and their contents included, subject to the
  same ignore rules.)

- If there is a `.gitignore` or `.npmignore` file, then ignored files in
  that and all child directories will be excluded from the package.  If
  _both_ files exist, then the `.gitignore` is ignored, and only the
  `.npmignore` is used.

  `.npmignore` files follow the [same pattern
  rules](https://git-scm.com/book/en/v2/Git-Basics-Recording-Changes-to-the-Repository#_ignoring)
  as `.gitignore` files

- If the file matches certain patterns, then it will _never_ be included,
  unless explicitly added to the `"files"` list in `package.json`, or
  un-ignored with a `!` rule in a `.npmignore` or `.gitignore` file.

- Symbolic links are never included in npm packages.


See [`developers`](/using-npm/developers) for full details on what's
included in the published package, as well as details on how the package is
built.

### See Also

* [npm-packlist package](http://npm.im/npm-packlist)
* [npm registry](/using-npm/registry)
* [npm scope](/using-npm/scope)
* [npm adduser](/commands/npm-adduser)
* [npm owner](/commands/npm-owner)
* [npm deprecate](/commands/npm-deprecate)
* [npm dist-tag](/commands/npm-dist-tag)
* [npm pack](/commands/npm-pack)
* [npm profile](/commands/npm-profile)
