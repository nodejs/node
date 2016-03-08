# npm-package-arg

Parse package name and specifier passed to commands like `npm install` or
`npm cache add`.  This just parses the text given-- it's worth noting that
`npm` has further logic it applies by looking at your disk to figure out
what ambiguous specifiers are.  If you want that logic, please see
[realize-package-specifier].

[realize-package-specifier]: https://www.npmjs.org/package/realize-package-specifier

Arguments look like: `foo@1.2`, `@bar/foo@1.2`, `foo@user/foo`, `http://x.com/foo.tgz`,
`git+https://github.com/user/foo`, `bitbucket:user/foo`, `foo.tar.gz` or `bar`

## EXAMPLES

```javascript
var assert = require("assert")
var npa = require("npm-package-arg")

// Pass in the descriptor, and it'll return an object
var parsed = npa("@bar/foo@1.2")

// Returns an object like:
{
  raw: '@bar/foo@1.2',   // what was passed in
  name: "@bar/foo",      // the name of the package
  scope: "@bar",         // the private scope of the package, or null
  type: "range",         // the type of specifier this is
  spec: ">=1.2.0 <1.3.0" // the expanded specifier
  rawSpec: "1.2"         // the specifier as passed in
 }

// Parsing urls pointing at hosted git services produces a variation:
var parsed = npa("git+https://github.com/user/foo")

// Returns an object like:
{
  raw: 'git+https://github.com/user/foo',
  scope: null,
  name: null,
  rawSpec: 'git+https://github.com/user/foo',
  spec: 'user/foo',
  type: 'hosted',
  hosted: {
    type: 'github',
    ssh: 'git@github.com:user/foo.git',
    sshurl: 'git+ssh://git@github.com/user/foo.git',
    https: 'https://github.com/user/foo.git',
    directUrl: 'https://raw.githubusercontent.com/user/foo/master/package.json'
  }
}

// Completely unreasonable invalid garbage throws an error
// Make sure you wrap this in a try/catch if you have not
// already sanitized the inputs!
assert.throws(function() {
  npa("this is not \0 a valid package name or url")
})
```

## USING

`var npa = require('npm-package-arg')`

* var result = npa(*arg*)

Parses *arg* and returns a result object detailing what *arg* is.

*arg* -- a package descriptor, like: `foo@1.2`, or `foo@user/foo`, or
`http://x.com/foo.tgz`, or `git+https://github.com/user/foo`

## RESULT OBJECT

The objects that are returned by npm-package-arg contain the following
keys:

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
  * `local` - A local file or folder path
  * `remote` - An http url (presumably to a tgz)
* `spec` - The "thing".  URL, the range, git repo, etc.
* `hosted` - If type=hosted this will be an object with the following keys:
  * `type` - github, bitbucket or gitlab
  * `ssh` - The ssh path for this git repo
  * `sshUrl` - The ssh URL for this git repo
  * `httpsUrl` - The HTTPS URL for this git repo
  * `directUrl` - The URL for the package.json in this git repo
* `raw` - The original un-modified string that was provided.
* `rawSpec` - The part after the `name@...`, as it was originally
  provided.
* `scope` - If a name is something like `@org/module` then the `scope`
  field will be set to `org`.  If it doesn't have a scoped name, then
  scope is `null`.

If you only include a name and no specifier part, eg, `foo` or `foo@` then
a default of `latest` will be used (as of 4.1.0). This is contrast with
previous behavior where `*` was used.
