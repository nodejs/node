var tap = require('tap')
  , test = tap.test
  , util = require('util')

// Issue #10 is about missing log4js. This file sets the environment variable to disable it.
process.env.log_plain = true

var lib = require('../../lib')
  , couch = require('../couch')
  , follow = require('../../api')

couch.setup(test)

test('Issue #10', function(t) {
  follow({db:couch.DB, inactivity_ms:30000}, function(er, change) {
    console.error('Change: ' + JSON.stringify(change))
    if(change.seq == 2)
      this.stop()

    this.on('stop', function() {
      t.end()
    })
  })
})
