/* global describe, it */

var exec = require('child_process').exec,
  assert = require('assert')

require('chai').should()
require('tap').mochaGlobals()

var onSignalExit = require('../')

describe('all-signals-integration-test', function () {

  // These are signals that are aliases for other signals, so
  // the result will sometimes be one of the others.  For these,
  // we just verify that we GOT a signal, not what it is.
  function weirdSignal (sig) {
    return sig === 'SIGIOT' ||
      sig === 'SIGIO' ||
      sig === 'SIGSYS' ||
      sig === 'SIGIOT' ||
      sig === 'SIGABRT' ||
      sig === 'SIGPOLL' ||
      sig === 'SIGUNUSED'
  }

  // Exhaustively test every signal, and a few numbers.
  var signals = onSignalExit.signals()
  signals.concat('', 0, 1, 2, 3, 54).forEach(function (sig) {
    var node = process.execPath
    var js = require.resolve('./fixtures/exiter.js')
    it('exits properly: ' + sig, function (done) {
      // travis has issues with SIGUSR1 on Node 0.x.10.
      if (process.env.TRAVIS && sig === 'SIGUSR1') return done()

      exec(node + ' ' + js + ' ' + sig, function (err, stdout, stderr) {
        if (sig) {
          assert(err)
          if (!isNaN(sig)) {
            assert.equal(err.code, sig)
          } else if (!weirdSignal(sig)) {
            if (!process.env.TRAVIS) err.signal.should.equal(sig)
          } else if (sig) {
            if (!process.env.TRAVIS) assert(err.signal)
          }
        } else {
          assert.ifError(err)
        }

        try {
          var data = JSON.parse(stdout)
        } catch (er) {
          console.error('invalid json: %j', stdout, stderr)
          throw er
        }

        if (weirdSignal(sig)) {
          data.wanted[1] = true
          data.found[1] = !!data.found[1]
        }
        assert.deepEqual(data.found, data.wanted)
        done()
      })
    })
  })

  signals.forEach(function (sig) {
    var node = process.execPath
    var js = require.resolve('./fixtures/parent.js')
    it('exits properly: (external sig) ' + sig, function (done) {
      // travis has issues with SIGUSR1 on Node 0.x.10.
      if (process.env.TRAVIS && sig === 'SIGUSR1') return done()

      var cmd = node + ' ' + js + ' ' + sig
      exec(cmd, function (err, stdout, stderr) {
        assert.ifError(err)
        try {
          var data = JSON.parse(stdout)
        } catch (er) {
          console.error('invalid json: %j', stdout, stderr)
          throw er
        }

        if (weirdSignal(sig)) {
          data.wanted[1] = true
          data.found[1] = !!data.found[1]
          data.external[1] = !!data.external[1]
        }
        assert.deepEqual(data.found, data.wanted)
        assert.deepEqual(data.external, data.wanted)
        done()
      })
    })
  })
})
