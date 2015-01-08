# normalize-git-url

You have a bunch of Git URLs. You want to convert them to a canonical
representation, probably for use inside npm so that it doesn't end up creating
a bunch of superfluous cached origins. You use this package.

## Usage

```javascript
var ngu = require('normalize-git-url');
var normalized = ngu("git+ssh://git@github.com:organization/repo.git#hashbrowns")
// get back:
// {
//   url : "ssh://git@github.com/organization/repo.git",
//   branch : "hashbrowns" // did u know hashbrowns are delicious?
// }
```

## API

There's just the one function, and all it takes is a single parameter, a non-normalized Git URL.

### normalizeGitUrl(url)

* `url` {String} The Git URL (very loosely speaking) to be normalized.

Returns an object with the following format:

* `url` {String} The normalized URL.
* `branch` {String} The treeish to be checked out once the repo at `url` is
  cloned. It doesn't have to be a branch, but it's a lot easier to intuit what
  the output is for with that name.

## Limitations

Right now this doesn't try to special-case GitHub too much -- it doesn't ensure
that `.git` is added to the end of URLs, it doesn't prefer `https:` over
`http:` or `ssh:`, it doesn't deal with redirects, and it doesn't try to
resolve symbolic names to treeish hashcodes. For now, it just tries to account
for minor differences in representation.
