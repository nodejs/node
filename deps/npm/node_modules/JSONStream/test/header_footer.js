

var fs = require ('fs')
  , join = require('path').join
  , file = join(__dirname, 'fixtures','header_footer.json')
  , JSONStream = require('../')
  , it = require('it-is')

var expected = JSON.parse(fs.readFileSync(file))
  , parser = JSONStream.parse(['rows', /\d+/ /*, 'value'*/])
  , called = 0
  , headerCalled = 0
  , footerCalled = 0
  , ended = false
  , parsed = []

fs.createReadStream(file).pipe(parser)

parser.on('header', function (data) {
  headerCalled ++
  it(data).deepEqual({
    total_rows: 129,
    offset: 0
  })
})

parser.on('footer', function (data) {
  footerCalled ++
  it(data).deepEqual({
    foo: { bar: 'baz' }
  })
})

parser.on('data', function (data) {
  called ++
  it.has({
    id: it.typeof('string'),
    value: {rev: it.typeof('string')},
    key:it.typeof('string')
  })
  it(headerCalled).equal(1)
  parsed.push(data)
})

parser.on('end', function () {
  ended = true
})

process.on('exit', function () {
  it(called).equal(expected.rows.length)
  it(headerCalled).equal(1)
  it(footerCalled).equal(1)
  it(parsed).deepEqual(expected.rows)
  console.error('PASSED')
})
