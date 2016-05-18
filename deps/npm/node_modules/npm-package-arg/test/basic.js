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
      spec: "latest",
      type: "tag"
    },

    "@foo/bar@": {
      raw: "@foo/bar@",
      name: "@foo/bar",
      scope: "@foo",
      rawSpec: "",
      spec: "latest",
      type: "tag"
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
      spec: "@f fo o al/ a d s ;f",
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

    "git+ssh://git@notgithub.com/user/foo#1.2.3": {
      name: null,
      type: "git",
      spec: "ssh://git@notgithub.com/user/foo#1.2.3",
      raw: "git+ssh://git@notgithub.com/user/foo#1.2.3"
    },

    "git+file://path/to/repo#1.2.3": {
      name: null,
      type: "git",
      spec: "file://path/to/repo#1.2.3",
      raw: "git+file://path/to/repo#1.2.3"
    },

    "git://notgithub.com/user/foo": {
      name: null,
      type: "git",
      spec: "git://notgithub.com/user/foo",
      raw: "git://notgithub.com/user/foo"
    },

    "@foo/bar@git+ssh://notgithub.com/user/foo": {
      name: "@foo/bar",
      scope: "@foo",
      spec: "ssh://notgithub.com/user/foo",
      rawSpec: "git+ssh://notgithub.com/user/foo",
      raw: "@foo/bar@git+ssh://notgithub.com/user/foo"
    },

    "/path/to/foo": {
      name: null,
      type: "local",
      spec: path.resolve(__dirname, "/path/to/foo"),
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

    "foo@latest": {
      name: "foo",
      type: "tag",
      spec: "latest",
      raw: "foo@latest"
    },

    "foo": {
      name: "foo",
      type: "tag",
      spec: "latest",
      raw: "foo"
    }
  }

  Object.keys(tests).forEach(function (arg) {
    var res = npa(arg)
    t.type(res, "Result", arg + " is result")
    t.has(res, tests[arg], arg + " matches expectations")
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
