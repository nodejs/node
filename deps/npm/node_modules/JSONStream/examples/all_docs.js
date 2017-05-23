var request = require('request')
  , JSONStream = require('JSONStream')
  , es = require('event-stream')

var parser = JSONStream.parse(['rows', true]) //emit parts that match this path (any element of the rows array)
  , req = request({url: 'http://isaacs.couchone.com/registry/_all_docs'})
  , logger = es.mapSync(function (data) {  //create a stream that logs to stderr,
    console.error(data)
    return data  
  })

req.pipe(parser)
parser.pipe(logger)
