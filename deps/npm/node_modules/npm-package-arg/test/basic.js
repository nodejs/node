var npa = require("../npa.js")
var path = require("path")

require("tap").test("basic", function (t) {
  t.setMaxListeners(999)

  var tests = {
    "foo@1.2": {
      name: "foo",
      type: "range",
      spec: ">=1.2.0 <1.3.0",
      raw: "foo@1.2",
      rawSpec: "1.2"
    },

    "@foo/bar": {
      raw: "@foo/bar",
      name: "@foo/bar",
      scope: "@foo",
      rawSpec: "",
      spec: "*",
      type: "range"
    },

    "@foo/bar@": {
      raw: "@foo/bar@",
      name: "@foo/bar",
      scope: "@foo",
      rawSpec: "",
      spec: "*",
      type: "range"
    },

    "@foo/bar@baz": {
      raw: "@foo/bar@baz",
      name: "@foo/bar",
      scope: "@foo",
      rawSpec: "baz",
      spec: "baz",
      type: "tag"
    },

    "@f fo o al/ a d s ;f ": {
      raw: "@f fo o al/ a d s ;f",
      name: null,
      rawSpec: "@f fo o al/ a d s ;f",
      spec: path.resolve("@f fo o al/ a d s ;f"),
      type: "local"
    },

    "foo@1.2.3": {
      name: "foo",
      type: "version",
      spec: "1.2.3",
      raw: "foo@1.2.3"
    },

    "foo@=v1.2.3": {
      name: "foo",
      type: "version",
      spec: "1.2.3",
      raw: "foo@=v1.2.3",
      rawSpec: "=v1.2.3"
    },

    "git+ssh://git@github.com/user/foo#1.2.3": {
      name: null,
      type: "git",
      spec: "ssh://git@github.com/user/foo#1.2.3",
      raw: "git+ssh://git@github.com/user/foo#1.2.3"
    },

    "git+file://path/to/repo#1.2.3": {
      name: null,
      type: "git",
      spec: "file://path/to/repo#1.2.3",
      raw: "git+file://path/to/repo#1.2.3"
    },

    "git://github.com/user/foo": {
      name: null,
      type: "git",
      spec: "git://github.com/user/foo",
      raw: "git://github.com/user/foo"
    },

    "@foo/bar@git+ssh://github.com/user/foo": {
      name: "@foo/bar",
      scope: "@foo",
      spec: "ssh://github.com/user/foo",
      rawSpec: "git+ssh://github.com/user/foo",
      raw: "@foo/bar@git+ssh://github.com/user/foo"
    },

    "/path/to/foo": {
      name: null,
      type: "local",
      spec: "/path/to/foo",
      raw: "/path/to/foo"
    },

    "file:path/to/foo": {
      name: null,
      type: "local",
      spec: "path/to/foo",
      raw: "file:path/to/foo"
    },

    "file:~/path/to/foo": {
      name: null,
      type: "local",
      spec: "~/path/to/foo",
      raw: "file:~/path/to/foo"
    },

    "file:../path/to/foo": {
      name: null,
      type: "local",
      spec: "../path/to/foo",
      raw: "file:../path/to/foo"
    },

    "file:///path/to/foo": {
      name: null,
      type: "local",
      spec: "/path/to/foo",
      raw: "file:///path/to/foo"
    },

    "https://server.com/foo.tgz": {
      name: null,
      type: "remote",
      spec: "https://server.com/foo.tgz",
      raw: "https://server.com/foo.tgz"
    },

    "user/foo-js": {
      name: null,
      type: "github",
      spec: "user/foo-js",
      raw: "user/foo-js"
    },

    "user/foo-js#bar/baz": {
      name: null,
      type: "github",
      spec: "user/foo-js#bar/baz",
      raw: "user/foo-js#bar/baz"
    },

    "user..blerg--/..foo-js# . . . . . some . tags / / /": {
      name: null,
      type: "github",
      spec: "user..blerg--/..foo-js# . . . . . some . tags / / /",
      raw: "user..blerg--/..foo-js# . . . . . some . tags / / /"
    },

    "user/foo-js#bar/baz/bin": {
      name: null,
      type: "github",
      spec: "user/foo-js#bar/baz/bin",
      raw: "user/foo-js#bar/baz/bin"
    },

    "foo@user/foo-js": {
      name: "foo",
      type: "github",
      spec: "user/foo-js",
      raw: "foo@user/foo-js"
    },

    "foo@latest": {
      name: "foo",
      type: "tag",
      spec: "latest",
      raw: "foo@latest"
    },

    "foo": {
      name: "foo",
      type: "range",
      spec: "*",
      raw: "foo"
    }
  }

  Object.keys(tests).forEach(function (arg) {
    var res = npa(arg)
    t.type(res, "Result")
    t.has(res, tests[arg])
  })

  // Completely unreasonable invalid garbage throws an error
  t.throws(function() {
    npa("this is not a \0 valid package name or url")
  })

  t.throws(function() {
    npa("gopher://yea right")
  }, "Unsupported URL Type: gopher://yea right")

  t.end()
})
