'use strict'
var test = require('tap').test
var requireInject = require('require-inject')

var mockNpm = {
  config: {
    get: function (key) {
      return false
    }
  },
  commands: {
    outdated: function (args, silent, cb) {
      cb(null, [
        [{path: '/incorrect', parent: {path: '/correct'}}, 'abc', '1.0.0', '1.1.0', '1.1.0', '^1.1.0']
      ])
    }
  }
}

// What we're testing here is that updates use the parent module's path to
// install from.
test('update', function (t) {
  var update = requireInject('../../lib/update.js', {
    '../../lib/npm.js': mockNpm,
    '../../lib/install.js': {
      'Installer': function (where, dryrun, args) {
        t.is(where, '/correct', 'We should be installing to the parent of the modules being updated')
        this.run = function (cb) { cb() }
      }
    }
  })
  update(['abc'], function () {
    t.end()
  })
})
