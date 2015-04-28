var concat = require('../')
var test = require('tape')

test('writing objects', function (t) {
  var stream = concat({encoding: "objects"}, concatted)
  function concatted(objs) {
    t.equal(objs.length, 2)
    t.deepEqual(objs[0], {"foo": "bar"})
    t.deepEqual(objs[1], {"baz": "taco"})
  }
  stream.write({"foo": "bar"})
  stream.write({"baz": "taco"})
  stream.end()
  t.end()
})


test('switch to objects encoding if no encoding specified and objects are written', function (t) {
  var stream = concat(concatted)
  function concatted(objs) {
    t.equal(objs.length, 2)
    t.deepEqual(objs[0], {"foo": "bar"})
    t.deepEqual(objs[1], {"baz": "taco"})
  }
  stream.write({"foo": "bar"})
  stream.write({"baz": "taco"})
  stream.end()
  t.end()
})
