

var fs = require ('fs')
  , join = require('path').join
  , file = join(__dirname, 'fixtures','all_npm.json')
  , JSONStream = require('../')
  , it = require('it-is')

var expected = JSON.parse(fs.readFileSync(file))
  , parser = JSONStream.parse(['rows', /\d+/ /*, 'value'*/])
  , called = 0
  , ended = false
  , parsed = []

fs.createReadStream(file).pipe(parser)

parser.on('data', function (data) {
  called ++
  it.has({
    id: it.typeof('string'),
    value: {rev: it.typeof('string')},
    key:it.typeof('string')
  })
  parsed.push(data)
})

parser.on('end', function () {
  ended = true
})

process.on('exit', function () {
  it(called).equal(expected.rows.length)
  it(parsed).deepEqual(expected.rows)
  console.error('PASSED')
})
