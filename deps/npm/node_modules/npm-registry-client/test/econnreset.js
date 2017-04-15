'use strict'
var requireInject = require('require-inject')
var test = require('tap').test
var EventEmitter = require('events').EventEmitter
var PassThrough = require('readable-stream').PassThrough

var content = [
  'first chunk',
  'second chunk'
]
var requests = 0
var fetch = requireInject('../lib/fetch.js', {
  request: function (opts) {
    var req = new EventEmitter()
    ++requests
    setTimeout(function () {
      var res = new PassThrough()
      res.statusCode = 200

      setTimeout(function () {
        res.write(content[0])
      }, 50)

      if (requests === 1) {
        setTimeout(function () {
          var err = new Error('read ECONNRESET')
          err.code = 'ECONNRESET'
          req.emit('error', err)
        }, 100)
      } else {
        // we allow success on retries, though in practice we won't be
        // retrying.
        setTimeout(function () {
          res.end(content[1])
        }, 100)
      }
      req.emit('response', res)
    }, 50)
    return req
  }
})

function clientMock (t) {
  return {
    log: {
      info: function (section, message) {
        t.comment('[info] ' + section + ': ' + [].slice.call(arguments, 1).join(' '))
      },
      http: function (section, message) {
        t.comment('[http] ' + section + ': ' + [].slice.call(arguments, 1).join(' '))
      }
    },
    authify: function (alwaysAuth, parsed, headers, auth) {
      return
    },
    initialize: function (parsed, method, accept, headers) {
      return {}
    },
    attempt: require('../lib/attempt.js'),
    config: {
      retry: {
        retries: 2,
        factor: 10,
        minTimeout: 10000,
        maxTimeout: 60000
      }
    }
  }
}

/*
This tests that errors that occur in the REQUEST object AFTER a `response`
event has been emitted will be passed through to the `response` stream.
This means that we won't try to retry these sorts of errors ourselves.
*/

test('econnreset', function (t) {
  var client = clientMock(t)
  fetch.call(client, 'http://example.com/', {}, function (err, res) {
    t.ifError(err, 'initial fetch ok')
    var data = ''
    res.on('data', function (chunk) {
      data += chunk
    })
    res.on('error', function (err) {
      t.comment('ERROR:', err.stack)
      t.pass("errored and that's ok")
      t.done()
    })
    res.on('end', function () {
      t.is(data, content.join(''), 'succeeded and got the right data')
      t.done()
    })
  })
})
