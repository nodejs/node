var test = require ('tap') . test
var readdir = require ('../readdir.js')

test ('basic', function (t) {
  // should not get {a,b}/{x,y}, but SHOULD get @org/ and @scope children
  var expect = [ '@org/x', '@org/y', '@scope/x', '@scope/y', 'a', 'b' ]

  readdir (__dirname + '/fixtures', function (er, kids) {
    if (er)
      throw er
    t.same(kids, expect)
    t.end()
  })
})
