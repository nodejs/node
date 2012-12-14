var test = require("tap").test
var glob = require('../')
process.chdir(__dirname)

test("mark, no / on pattern", function (t) {
  glob("a/*", {mark: true}, function (er, results) {
    if (er)
      throw er
    t.same(results, [ 'a/abcdef/',
                      'a/abcfed/',
                      'a/b/',
                      'a/bc/',
                      'a/c/',
                      'a/cb/',
                      'a/symlink/' ])
    t.end()
  })
})

test("mark=false, no / on pattern", function (t) {
  glob("a/*", function (er, results) {
    if (er)
      throw er
    t.same(results, [ 'a/abcdef',
                      'a/abcfed',
                      'a/b',
                      'a/bc',
                      'a/c',
                      'a/cb',
                      'a/symlink' ])
    t.end()
  })
})

test("mark=true, / on pattern", function (t) {
  glob("a/*/", {mark: true}, function (er, results) {
    if (er)
      throw er
    t.same(results, [ 'a/abcdef/',
                      'a/abcfed/',
                      'a/b/',
                      'a/bc/',
                      'a/c/',
                      'a/cb/',
                      'a/symlink/' ])
    t.end()
  })
})

test("mark=false, / on pattern", function (t) {
  glob("a/*/", function (er, results) {
    if (er)
      throw er
    t.same(results, [ 'a/abcdef/',
                      'a/abcfed/',
                      'a/b/',
                      'a/bc/',
                      'a/c/',
                      'a/cb/',
                      'a/symlink/' ])
    t.end()
  })
})
