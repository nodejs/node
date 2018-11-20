

var fs = require ('fs')
  , join = require('path').join
  , file = join(__dirname, 'fixtures','error.json')
  , JSONStream = require('../')
  , it = require('it-is')

var expected = JSON.parse(fs.readFileSync(file))
  , parser = JSONStream.parse(['rows'])
  , called = 0
  , headerCalled = 0
  , footerCalled = 0
  , ended = false
  , parsed = []

fs.createReadStream(file).pipe(parser)

parser.on('header', function (data) {
  headerCalled ++
  it(data).deepEqual({
    error: 'error_code',
    message: 'this is an error message'
  })
})

parser.on('footer', function (data) {
  footerCalled ++
})

parser.on('data', function (data) {
  called ++
  parsed.push(data)
})

parser.on('end', function () {
  ended = true
})

process.on('exit', function () {
  it(called).equal(0)
  it(headerCalled).equal(1)
  it(footerCalled).equal(0)
  console.error('PASSED')
})
