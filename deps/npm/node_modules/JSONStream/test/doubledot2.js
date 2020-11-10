 var fs = require ('fs')
   , join = require('path').join
   , file = join(__dirname, 'fixtures','depth.json')
   , JSONStream = require('../')
   , it = require('it-is')

 var expected = JSON.parse(fs.readFileSync(file))
   , parser = JSONStream.parse(['docs', {recurse: true}, 'value'])
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
   var expectedValues = [0, [1], {"a": 2}, "3", 4]
   it(called).equal(expectedValues.length)
   for (var i = 0 ; i < 5 ; i++)
     it(parsed[i]).deepEqual(expectedValues[i])
   console.error('PASSED')
 })
