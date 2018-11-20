var JSONStream = require('../')
  , stream = require('stream')
  , it = require('it-is')

var output = [ [], [] ]

var parser1 = JSONStream.parse(['docs', /./])
parser1.on('data', function(data) {
  output[0].push(data)
})

var parser2 = JSONStream.parse(['docs', /./])
parser2.on('data', function(data) {
  output[1].push(data)
})

var pending = 2
function onend () {
  if (--pending > 0) return
  it(output).deepEqual([
    [], [{hello: 'world'}]
  ])
  console.error('PASSED')
}
parser1.on('end', onend)
parser2.on('end', onend)

function makeReadableStream() {
  var readStream = new stream.Stream()
  readStream.readable = true
  readStream.write = function (data) { this.emit('data', data) }
  readStream.end = function (data) { this.emit('end') }
  return readStream
}

var emptyArray = makeReadableStream()
emptyArray.pipe(parser1)
emptyArray.write('{"docs":[]}')
emptyArray.end()

var objectArray = makeReadableStream()
objectArray.pipe(parser2)
objectArray.write('{"docs":[{"hello":"world"}]}')
objectArray.end()
