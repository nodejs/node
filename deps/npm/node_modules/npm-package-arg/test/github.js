var npa = require("../npa.js")
var path = require("path")

require("tap").test("basic", function (t) {
  t.setMaxListeners(999)

  var tests = {
    "user/foo-js": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      spec: "github:user/foo-js",
      raw: "user/foo-js"
    },

    "user/foo-js#bar/baz": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      spec: "github:user/foo-js#bar/baz",
      raw: "user/foo-js#bar/baz"
    },

    "user..blerg--/..foo-js# . . . . . some . tags / / /": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      spec: "github:user..blerg--/..foo-js# . . . . . some . tags / / /",
      raw: "user..blerg--/..foo-js# . . . . . some . tags / / /"
    },

    "user/foo-js#bar/baz/bin": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      raw: "github:user/foo-js#bar/baz/bin",
      raw: "user/foo-js#bar/baz/bin"
    },

    "foo@user/foo-js": {
      name: "foo",
      type: "hosted",
      hosted: { type: "github" },
      spec: "github:user/foo-js",
      raw: "foo@user/foo-js"
    },

    "github:user/foo-js": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      spec: "github:user/foo-js",
      raw: "github:user/foo-js"
    },

    "git+ssh://git@github.com/user/foo#1.2.3": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      spec: "git+ssh://git@github.com/user/foo.git#1.2.3",
      raw: "git+ssh://git@github.com/user/foo#1.2.3"
    },

    "git://github.com/user/foo": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      spec: "git://github.com/user/foo.git",
      raw: "git://github.com/user/foo"
    },

    "https://github.com/user/foo.git": {
      name: null,
      type: "hosted",
      hosted: { type: "github" },
      spec: "git+https://github.com/user/foo.git",
      raw: "https://github.com/user/foo.git"
    },

    "@foo/bar@git+ssh://github.com/user/foo": {
      name: "@foo/bar",
      scope: "@foo",
      type: "hosted",
      hosted: { type: "github" },
      spec: "git+ssh://git@github.com/user/foo.git",
      rawSpec: "git+ssh://github.com/user/foo",
      raw: "@foo/bar@git+ssh://github.com/user/foo"
    },

   "foo@bar/foo": {
     name: "foo",
     type: "hosted",
     hosted: { type: "github" },
     spec: "github:bar/foo",
     raw: "foo@bar/foo"
   }
  }

  Object.keys(tests).forEach(function (arg) {
    var res = npa(arg)
    t.type(res, "Result", arg + " is a result")
    t.has(res, tests[arg], arg + " matches expectations")
  })

  t.end()
})
