# npm-package-arg

[![Build Status](https://travis-ci.org/npm/npm-package-arg.svg?branch=master)](https://travis-ci.org/npm/npm-package-arg)

Parses package name and specifier passed to commands like `npm install` or
`npm cache add`, or as found in `package.json` dependency sections.

## EXAMPLES

```javascript
var assert = require("assert")
var npa = require("npm-package-arg")

// Pass in the descriptor, and it'll return an object
try {
  var parsed = npa("@bar/foo@1.2")
} catch (ex) {
  …
}
```

## USING

`var npa = require('npm-package-arg')`

### var result = npa(*arg*[, *where*])

* *arg* - a string that you might pass to `npm install`, like:
`foo@1.2`, `@bar/foo@1.2`, `foo@user/foo`, `http://x.com/foo.tgz`,
`git+https://github.com/user/foo`, `bitbucket:user/foo`, `foo.tar.gz`,
`../foo/bar/` or `bar`.  If the *arg* you provide doesn't have a specifier
part, eg `foo` then the specifier will default to `latest`.
* *where* - Optionally the path to resolve file paths relative to. Defaults to `process.cwd()`

**Throws** if the package name is invalid, a dist-tag is invalid or a URL's protocol is not supported.

### var result = npa.resolve(*name*, *spec*[, *where*])

* *name* - The name of the module you want to install. For example: `foo` or `@bar/foo`.
* *spec* - The specifier indicating where and how you can get this module. Something like:
`1.2`, `^1.7.17`, `http://x.com/foo.tgz`, `git+https://github.com/user/foo`,
`bitbucket:user/foo`, `file:foo.tar.gz` or `file:../foo/bar/`.  If not
included then the default is `latest`.
* *where* - Optionally the path to resolve file paths relative to. Defaults to `process.cwd()`

**Throws** if the package name is invalid, a dist-tag is invalid or a URL's protocol is not supported.

## RESULT OBJECT

The objects that are returned by npm-package-arg contain the following
keys:

* `type` - One of the following strings:
  * `git` - A git repo
  * `tag` - A tagged version, like `"foo@latest"`
  * `version` - A specific version number, like `"foo@1.2.3"`
  * `range` - A version range, like `"foo@2.x"`
  * `file` - A local `.tar.gz`, `.tar` or `.tgz` file.
  * `directory` - A local directory.
  * `remote` - An http url (presumably to a tgz)
  * `alias` - A specifier with an alias, like `myalias@npm:foo@1.2.3`
* `registry` - If true this specifier refers to a resource hosted on a
  registry.  This is true for `tag`, `version` and `range` types.
* `name` - If known, the `name` field expected in the resulting pkg.
* `scope` - If a name is something like `@org/module` then the `scope`
  field will be set to `@org`.  If it doesn't have a scoped name, then
  scope is `null`.
* `escapedName` - A version of `name` escaped to match the npm scoped packages
  specification. Mostly used when making requests against a registry. When
  `name` is `null`, `escapedName` will also be `null`.
* `rawSpec` - The specifier part that was parsed out in calls to `npa(arg)`,
  or the value of `spec` in calls to `npa.resolve(name, spec).
* `saveSpec` - The normalized specifier, for saving to package.json files.
  `null` for registry dependencies.
* `fetchSpec` - The version of the specifier to be used to fetch this
  resource.  `null` for shortcuts to hosted git dependencies as there isn't
  just one URL to try with them.
* `gitRange` - If set, this is a semver specifier to match against git tags with
* `gitCommittish` - If set, this is the specific committish to use with a git dependency.
* `hosted` - If `from === 'hosted'` then this will be a `hosted-git-info`
  object. This property is not included when serializing the object as
  JSON.
* `raw` - The original un-modified string that was provided.  If called as
  `npa.resolve(name, spec)` then this will be `name + '@' + spec`.
* `subSpec` - If `type === 'alias'`, this is a Result Object for parsing the
  target specifier for the alias.
