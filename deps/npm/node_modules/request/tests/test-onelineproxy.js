var http = require('http')
  , assert = require('assert')
  , request = require('../index')
  ;

var server = http.createServer(function (req, resp) {
  resp.statusCode = 200
  if (req.url === '/get') {
    assert.equal(req.method, 'GET')
    resp.write('content')
    resp.end()
    return
  }
  if (req.url === '/put') {
    var x = ''
    assert.equal(req.method, 'PUT')
    req.on('data', function (chunk) {
      x += chunk
    })
    req.on('end', function () {
      assert.equal(x, 'content')
      resp.write('success')
      resp.end()
    })
    return
  }
  if (req.url === '/proxy') {
    assert.equal(req.method, 'PUT')
    return req.pipe(request('http://localhost:8080/put')).pipe(resp)
  }

  if (req.url === '/test') {
    return request('http://localhost:8080/get').pipe(request.put('http://localhost:8080/proxy')).pipe(resp)
  }
  throw new Error('Unknown url', req.url)
}).listen(8080, function () {
  request('http://localhost:8080/test', function (e, resp, body) {
    if (e) throw e
    if (resp.statusCode !== 200) throw new Error('statusCode not 200 ' + resp.statusCode)
    assert.equal(body, 'success')
    server.close()
  })
})



