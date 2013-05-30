// a test where we validate the siguature of the keys
// otherwise exactly the same as the ssl test

var server = require('./server')
  , assert = require('assert')
  , request = require('../main.js')
  , fs = require('fs')
  , path = require('path')
  , opts = { key: path.resolve(__dirname, 'ssl/ca/server.key')
           , cert: path.resolve(__dirname, 'ssl/ca/server.crt') }
  , s = server.createSSLServer(null, opts)
  , caFile = path.resolve(__dirname, 'ssl/ca/ca.crt')
  , ca = fs.readFileSync(caFile)

var tests =
  { testGet :
    { resp : server.createGetResponse("TESTING!")
    , expectBody: "TESTING!"
    }
  , testGetChunkBreak :
    { resp : server.createChunkResponse(
      [ new Buffer([239])
      , new Buffer([163])
      , new Buffer([191])
      , new Buffer([206])
      , new Buffer([169])
      , new Buffer([226])
      , new Buffer([152])
      , new Buffer([131])
      ])
    , expectBody: "Ω☃"
    }
  , testGetJSON :
    { resp : server.createGetResponse('{"test":true}', 'application/json')
    , json : true
    , expectBody: {"test":true}
    }
  , testPutString :
    { resp : server.createPostValidator("PUTTINGDATA")
    , method : "PUT"
    , body : "PUTTINGDATA"
    }
  , testPutBuffer :
    { resp : server.createPostValidator("PUTTINGDATA")
    , method : "PUT"
    , body : new Buffer("PUTTINGDATA")
    }
  , testPutJSON :
    { resp : server.createPostValidator(JSON.stringify({foo: 'bar'}))
    , method: "PUT"
    , json: {foo: 'bar'}
    }
  , testPutMultipart :
    { resp: server.createPostValidator(
        '--__BOUNDARY__\r\n' +
        'content-type: text/html\r\n' +
        '\r\n' +
        '<html><body>Oh hi.</body></html>' +
        '\r\n--__BOUNDARY__\r\n\r\n' +
        'Oh hi.' +
        '\r\n--__BOUNDARY__--'
        )
    , method: "PUT"
    , multipart:
      [ {'content-type': 'text/html', 'body': '<html><body>Oh hi.</body></html>'}
      , {'body': 'Oh hi.'}
      ]
    }
  }

s.listen(s.port, function () {

  var counter = 0

  for (i in tests) {
    (function () {
      var test = tests[i]
      s.on('/'+i, test.resp)
      test.uri = s.url + '/' + i
      test.strictSSL = true
      test.ca = ca
      test.headers = { host: 'testing.request.mikealrogers.com' }
      request(test, function (err, resp, body) {
        if (err) throw err
        if (test.expectBody) {
          assert.deepEqual(test.expectBody, body)
        }
        counter = counter - 1;
        if (counter === 0) {
          console.log(Object.keys(tests).length+" tests passed.")
          s.close()
        }
      })
      counter++
    })()
  }
})
