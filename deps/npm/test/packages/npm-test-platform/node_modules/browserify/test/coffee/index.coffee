bar = require './bar'
foo = require './foo.coffee'
baz = require './baz'

module.exports = foo(foo bar) + baz
