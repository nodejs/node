var tap = require('tap')
var pz = require('../promzard.js')
var spawn = require('child_process').spawn

tap.test('run the example using a buffer', function (t) {

  var example = require.resolve('../example/buffer.js')
  var node = process.execPath

  var expect =  {
    "name": "example",
    "version": "0.0.0",
    "description": "testing description",
    "main": "test-entry.js",
    "directories": {
      "example": "example",
      "test": "test"
    },
    "dependencies": {},
    "devDependencies": {
      "tap": "~0.2.5"
    },
    "scripts": {
      "test": "tap test/*.js"
    },
    "repository": {
      "type": "git",
      "url": "git://github.com/substack/example.git"
    },
    "homepage": "https://github.com/substack/example",
    "keywords": [
      "fugazi",
      "function",
      "waiting",
      "room"
    ],
    "author": {
      "name": "James Halliday",
      "email": "mail@substack.net",
      "url": "http://substack.net"
    },
    "license": "MIT",
    "engine": {
      "node": ">=0.6"
    }
  }

  var c = spawn(node, [example], { customFds: [-1,-1,-1] })
  var output = ''
  c.stdout.on('data', function (d) {
    output += d
    respond()
  })

  var actual = ''
  c.stderr.on('data', function (d) {
    actual += d
  })

  function respond () {
    if (output.match(/description: $/)) {
      c.stdin.write('testing description\n')
      return
    }
    if (output.match(/entry point: \(index\.js\) $/)) {
      c.stdin.write('test-entry.js\n')
      return
    }
    if (output.match(/keywords: $/)) {
      c.stdin.write('fugazi function waiting room\n')
      // "read" module is weird on node >= 0.10 when not a TTY
      // requires explicit ending for reasons.
      // could dig in, but really just wanna make tests pass, whatever.
      c.stdin.end()
      return
    }
  }

  c.on('close', function () {
    actual = JSON.parse(actual)
    t.deepEqual(actual, expect)
    t.end()
  })
})
