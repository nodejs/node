var test = require("tap").test

var normalize = require("../normalize-git-url.js")

test("basic normalization tests", function (t) {
  t.same(
    normalize("git+ssh://user@hostname:project.git#commit-ish"),
    { url : "ssh://user@hostname/project.git", branch : "commit-ish" }
  )
  t.same(
    normalize("git+http://user@hostname/project/blah.git#commit-ish"),
    { url : "http://user@hostname/project/blah.git", branch : "commit-ish" }
  )
  t.same(
    normalize("git+https://user@hostname/project/blah.git#commit-ish"),
    { url : "https://user@hostname/project/blah.git", branch : "commit-ish" }
  )
  t.same(
    normalize("git+ssh://git@github.com:npm/npm.git#v1.0.27"),
    { url : "ssh://git@github.com/npm/npm.git", branch : "v1.0.27" }
  )
  t.same(
    normalize("git+ssh://git@github.com:org/repo#dev"),
    { url : "ssh://git@github.com/org/repo", branch : "dev" }
  )
  t.same(
    normalize("git+ssh://git@github.com/org/repo#dev"),
    { url : "ssh://git@github.com/org/repo", branch : "dev" }
  )
  t.same(
    normalize("git+ssh://foo:22/some/path"),
    { url : "ssh://foo:22/some/path", branch : "master" }
  )
  t.same(
    normalize("git@github.com:org/repo#dev"),
    { url : "git@github.com:org/repo", branch : "dev" }
  )
  t.same(
    normalize("git+https://github.com/KenanY/node-uuid"),
    { url : "https://github.com/KenanY/node-uuid", branch : "master" }
  )
  t.same(
    normalize("git+https://github.com/KenanY/node-uuid#7a018f2d075b03a73409e8356f9b29c9ad4ea2c5"),
    { url : "https://github.com/KenanY/node-uuid", branch : "7a018f2d075b03a73409e8356f9b29c9ad4ea2c5" }
  )
  t.same(
    normalize("git+ssh://git@git.example.com:b/b.git#v1.0.0"),
    { url : "ssh://git@git.example.com/b/b.git", branch : "v1.0.0" }
  )
  t.same(
    normalize("git+ssh://git@github.com:npm/npm-proto.git#othiym23/organized"),
    { url : "ssh://git@github.com/npm/npm-proto.git", branch : "othiym23/organized" }
  )

  t.end()
})
