var tape = require('tape')
var from = require('from2')
var iterate = require('./')

tape('merge sort', function (t) {
  var a = from.obj(['a', 'b', 'd', 'e', 'g', 'h'])
  var b = from.obj(['b', 'c', 'f'])
  var output = []

  var readA = iterate(a)
  var readB = iterate(b)

  var loop = function () {
    readA(function (err, dataA, nextA) {
      if (err) throw err
      readB(function (err, dataB, nextB) {
        if (err) throw err

        if (!dataA && !dataB) {
          t.same(output, ['a', 'b', 'b', 'c', 'd', 'e', 'f', 'g', 'h'], 'sorts list')
          t.end()
          return
        }

        if (!dataB || dataA < dataB) {
          output.push(dataA)
          nextA()
          return loop()
        }

        if (!dataA || dataA > dataB) {
          output.push(dataB)
          nextB()
          return loop()
        }

        output.push(dataA)
        output.push(dataB)
        nextA()
        nextB()
        loop()
      })
    })
  }

  loop()
})

tape('error handling', function (t) {
  var a = from.obj(['a', 'b', 'd', 'e', 'g', 'h'])
  var read = iterate(a)

  a.destroy(new Error('oh no'))

  read(function (err) {
    t.ok(err, 'had error')
    t.same(err.message, 'oh no')
    t.end()
  })
})
