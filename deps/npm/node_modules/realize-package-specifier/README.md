realize-package-specifier
-------------------------

Parse a package specifier, peeking at the disk to differentiate between
local tarballs, directories and named modules.  This implements the logic
used by `npm install` and `npm cache` to determine where to get packages
from.

```javascript
var realizePackageSpecifier = require("realize-package-specifier")
realizePackageSpecifier("foo.tar.gz", ".", function (err, package) {
    …
})
```

## Using

* realizePackageSpecifier(*spec*, [*where*,] *callback*)

Parses *spec* using `npm-package-arg` and then uses stat to check to see if
it refers to a local tarball or package directory.  Stats are done relative
to *where*.  If it does then the local module is loaded.  If it doesn't then
target is left as a remote package specifier.  Package directories are
recognized by the presence of a package.json in them.

*spec* -- a package specifier, like: `foo@1.2`, or `foo@user/foo`, or
`http://x.com/foo.tgz`, or `git+https://github.com/user/foo`

*where* (optional, default: .) -- The directory in which we should look for
local tarballs or package directories.

*callback* function(*err*, *result*) -- Called once we've determined what
kind of specifier this is.  The *result* object will be very like the one
returned by `npm-package-arg` except with three differences: 1) There's a
new type of `directory`.  2) The `local` type only refers to tarballs.  2)
For all `local` and `directory` type results spec will contain the full path of
the local package.

## Result Object

The full definition of the result object is:

* `name` - If known, the `name` field expected in the resulting pkg.
* `type` - One of the following strings:
  * `git` - A git repo
  * `hosted` - A hosted project, from github, bitbucket or gitlab. Originally
    either a full url pointing at one of these services or a shorthand like
    `user/project` or `github:user/project` for github or `bitbucket:user/project`
    for bitbucket.
  * `tag` - A tagged version, like `"foo@latest"`
  * `version` - A specific version number, like `"foo@1.2.3"`
  * `range` - A version range, like `"foo@2.x"`
  * `local` - A local file path
  * `directory` - A local package directory
  * `remote` - An http url (presumably to a tgz)
* `spec` - The "thing".  URL, the range, git repo, etc.
* `hosted` - If type=hosted this will be an object with the following keys:
  * `type` - github, bitbucket or gitlab
  * `ssh` - The ssh path for this git repo
  * `sshurl` - The ssh URL for this git repo
  * `https` - The HTTPS URL for this git repo
  * `directUrl` - The URL for the package.json in this git repo
* `raw` - The original un-modified string that was provided.
* `rawSpec` - The part after the `name@...`, as it was originally
  provided.
* `scope` - If a name is something like `@org/module` then the `scope`
  field will be set to `org`.  If it doesn't have a scoped name, then
  scope is `null`.

