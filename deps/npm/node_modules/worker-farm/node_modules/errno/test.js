#!/usr/bin/env node

var test  = require('tape')
  , errno = require('./')

test('sanity checks', function (t) {
  t.ok(errno.all, 'errno.all not found')
  t.ok(errno.errno, 'errno.errno not found')
  t.ok(errno.code, 'errno.code not found')

  t.equal(errno.all.length, 59, 'found ' + errno.all.length + ', expected 59')
  t.equal(errno.errno['-1'], errno.all[0], 'errno -1 not first element')

  t.equal(errno.code['UNKNOWN'], errno.all[0], 'code UNKNOWN not first element')

  t.equal(errno.errno[1], errno.all[2], 'errno 1 not third element')

  t.equal(errno.code['EOF'], errno.all[2], 'code EOF not third element')
  t.end()
})

test('custom errors', function (t) {
  var Cust = errno.create('FooNotBarError')
  var cust = new Cust('foo is not bar')

  t.equal(cust.name, 'FooNotBarError', 'correct custom name')
  t.equal(cust.type, 'FooNotBarError', 'correct custom type')
  t.equal(cust.message, 'foo is not bar', 'correct custom message')
  t.notOk(cust.cause, 'no cause')
  t.end()
})
