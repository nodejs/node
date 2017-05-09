var fs = require ('fs')
  , join = require('path').join
  , file = join(__dirname, 'fixtures','all_npm.json')
  , JSONStream = require('../')
  , it = require('it-is')

var expected = JSON.parse(fs.readFileSync(file))
  , parser = JSONStream.parse('rows..rev')
  , called = 0
  , ended = false
  , parsed = []

fs.createReadStream(file).pipe(parser)
  
parser.on('data', function (data) {
  called ++
  parsed.push(data)
})

parser.on('end', function () {
  ended = true
})

process.on('exit', function () {
  it(called).equal(expected.rows.length)
  for (var i = 0 ; i < expected.rows.length ; i++)
    it(parsed[i]).deepEqual(expected.rows[i].value.rev)
  console.error('PASSED')
})
