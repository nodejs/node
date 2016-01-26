var test = require('tap').test
var writeStream = require('../index.js')
var fs = require('fs')
var path = require('path')

test('basic', function (t) {
  // open 10 write streams to the same file.
  // then write to each of them, and to the target
  // and verify at the end that each of them does their thing
  var target = path.resolve(__dirname, 'test.txt')
  var n = 10

  var streams = []
  for (var i = 0; i < n; i++) {
    var s = writeStream(target)
    s.on('finish', verifier('finish'))
    s.on('close', verifier('close'))
    streams.push(s)
  }

  var verifierCalled = 0
  function verifier (ev) {
    return function () {
      if (ev === 'close') {
        t.equal(this.__emittedFinish, true)
      } else {
        this.__emittedFinish = true
        t.equal(ev, 'finish')
      }

      // make sure that one of the atomic streams won.
      var res = fs.readFileSync(target, 'utf8')
      var lines = res.trim().split(/\n/)
      lines.forEach(function (line) {
        var first = lines[0].match(/\d+$/)[0]
        var cur = line.match(/\d+$/)[0]
        t.equal(cur, first)
      })

      var resExpr = /^first write \d+\nsecond write \d+\nthird write \d+\nfinal write \d+\n$/
      t.similar(res, resExpr)

      // should be called once for each close, and each finish
      if (++verifierCalled === n * 2) {
        t.end()
      }
    }
  }

  // now write something to each stream.
  streams.forEach(function (stream, i) {
    stream.write('first write ' + i + '\n')
  })

  // wait a sec for those writes to go out.
  setTimeout(function () {
    // write something else to the target.
    fs.writeFileSync(target, 'brutality!\n')

    // write some more stuff.
    streams.forEach(function (stream, i) {
      stream.write('second write ' + i + '\n')
    })

    setTimeout(function () {
      // Oops!  Deleted the file!
      fs.unlinkSync(target)

      // write some more stuff.
      streams.forEach(function (stream, i) {
        stream.write('third write ' + i + '\n')
      })

      setTimeout(function () {
        fs.writeFileSync(target, 'brutality TWO!\n')
        streams.forEach(function (stream, i) {
          stream.end('final write ' + i + '\n')
        })
      }, 50)
    }, 50)
  }, 50)
})

test('cleanup', function (t) {
  fs.readdirSync(__dirname).filter(function (f) {
    return f.match(/^test.txt/)
  }).forEach(function (file) {
    fs.unlinkSync(path.resolve(__dirname, file))
  })
  t.end()
})
