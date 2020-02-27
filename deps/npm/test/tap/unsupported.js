'use strict'
var test = require('tap').test
var unsupported = require('../../lib/utils/unsupported.js')

var versions = [
  //          broken unsupported
  ['v0.1.103', true, true],
  ['v0.2.0', true, true],
  ['v0.3.5', true, true],
  ['v0.4.7', true, true],
  ['v0.5.3', true, true],
  ['v0.6.17', true, true],
  ['v0.7.8', true, true],
  ['v0.8.28', true, true],
  ['v0.9.6', true, true],
  ['v0.10.48', true, true],
  ['v0.11.16', true, true],
  ['v0.12.9', true, true],
  ['v1.0.1', true, true],
  ['v1.6.0', true, true],
  ['v2.3.1', true, true],
  ['v3.0.0', true, true],
  ['v4.5.0', true, true],
  ['v4.8.4', true, true],
  ['v5.7.1', true, true],
  ['v6.8.1', false, false],
  ['v7.0.0-beta23', false, true],
  ['v7.2.3', false, true],
  ['v8.4.0', false, false],
  ['v9.3.0', false, false],
  ['v10.0.0-0', false, false],
  ['v11.0.0-0', false, false],
  ['v12.0.0-0', false, false],
  ['v13.0.0-0', false, false]
]

test('versions', function (t) {
  t.plan(versions.length * 2)
  versions.forEach(function (verinfo) {
    var version = verinfo[0]
    var broken = verinfo[1]
    var unsupp = verinfo[2]
    var nodejs = unsupported.checkVersion(version)
    t.is(nodejs.broken, broken, version + ' ' + (broken ? '' : 'not ') + 'broken')
    t.is(nodejs.unsupported, unsupp, version + ' ' + (unsupp ? 'unsupported' : 'supported'))
  })
  t.done()
})
