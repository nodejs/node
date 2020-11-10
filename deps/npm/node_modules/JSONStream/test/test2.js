

var fs = require ('fs')
  , join = require('path').join
  , file = join(__dirname, '..','package.json')
  , JSONStream = require('../')
  , it = require('it-is')

var expected = JSON.parse(fs.readFileSync(file))
  , parser = JSONStream.parse([])
  , called = 0
  , ended = false
  , parsed = []

fs.createReadStream(file).pipe(parser)
  
parser.on('data', function (data) {
  called ++
  it(data).deepEqual(expected)
})

parser.on('end', function () {
  ended = true
})

process.on('exit', function () {
  it(called).equal(1)
  console.error('PASSED')
})