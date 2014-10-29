# npm-package-arg

Parse the things that can be arguments to `npm install`

Takes an argument like `foo@1.2`, or `foo@user/foo`, or
`http://x.com/foo.tgz`, or `git+https://github.com/user/foo`, and
figures out what type of thing it is.

## USAGE

```javascript
var assert = require("assert")
var npa = require("npm-package-arg")

// Pass in the descriptor, and it'll return an object
var parsed = npa("foo@1.2")

// Returns an object like:
// {
//  name: "foo",  // The bit in front of the @
//  type: "range", // the type of descriptor this is
//  spec: "1.2" // the specifier for this descriptor
// }

// Completely unreasonable invalid garbage throws an error
// Make sure you wrap this in a try/catch if you have not
// already sanitized the inputs!
assert.throws(function() {
  npa("this is not \0 a valid package name or url")
})
```

For more examples, see the test file.

## Result Objects

The objects that are returned by npm-package-arg contain the following
fields:

* `name` - If known, the `name` field expected in the resulting pkg.
* `type` - One of the following strings:
  * `git` - A git repo
  * `github` - A github shorthand, like `user/project`
  * `tag` - A tagged version, like `"foo@latest"`
  * `version` - A specific version number, like `"foo@1.2.3"`
  * `range` - A version range, like `"foo@2.x"`
  * `local` - A local file or folder path
  * `remote` - An http url (presumably to a tgz)
* `spec` - The "thing".  URL, the range, git repo, etc.
* `raw` - The original un-modified string that was provided.
* `rawSpec` - The part after the `name@...`, as it was originally
  provided.
* `scope` - If a name is something like `@org/module` then the `scope`
  field will be set to `org`.  If it doesn't have a scoped name, then
  scope is `null`.
