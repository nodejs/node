var http = require('http')
  , https = require('https')
  , server = require('./server')
  , assert = require('assert')
  , request = require('../index')


var faux_requests_made = {'http':0, 'https':0}
function wrap_request(name, module) {
  // Just like the http or https module, but note when a request is made.
  var wrapped = {}
  Object.keys(module).forEach(function(key) {
    var value = module[key];

    if(key != 'request')
      wrapped[key] = value;
    else
      wrapped[key] = function(options, callback) {
        faux_requests_made[name] += 1
        return value.apply(this, arguments)
      }
  })

  return wrapped;
}


var faux_http = wrap_request('http', http)
  , faux_https = wrap_request('https', https)
  , plain_server = server.createServer()
  , https_server = server.createSSLServer()


plain_server.listen(plain_server.port, function() {
  plain_server.on('/plain', function (req, res) {
    res.writeHead(200)
    res.end('plain')
  })
  plain_server.on('/to_https', function (req, res) {
    res.writeHead(301, {'location':'https://localhost:'+https_server.port + '/https'})
    res.end()
  })

  https_server.listen(https_server.port, function() {
    https_server.on('/https', function (req, res) {
      res.writeHead(200)
      res.end('https')
    })
    https_server.on('/to_plain', function (req, res) {
      res.writeHead(302, {'location':'http://localhost:'+plain_server.port + '/plain'})
      res.end()
    })

    run_tests()
    run_tests({})
    run_tests({'http:':faux_http})
    run_tests({'https:':faux_https})
    run_tests({'http:':faux_http, 'https:':faux_https})
  })
})

function run_tests(httpModules) {
  var to_https = 'http://localhost:'+plain_server.port+'/to_https'
  var to_plain = 'https://localhost:'+https_server.port+'/to_plain'

  request(to_https, {'httpModules':httpModules, strictSSL:false}, function (er, res, body) {
    if (er) throw er
    assert.equal(body, 'https', 'Received HTTPS server body')
    done()
  })

  request(to_plain, {'httpModules':httpModules, strictSSL:false}, function (er, res, body) {
    if (er) throw er
    assert.equal(body, 'plain', 'Received HTTPS server body')
    done()
  })
}


var passed = 0;
function done() {
  passed += 1
  var expected = 10

  if(passed == expected) {
    plain_server.close()
    https_server.close()

    assert.equal(faux_requests_made.http, 4, 'Wrapped http module called appropriately')
    assert.equal(faux_requests_made.https, 4, 'Wrapped https module called appropriately')

    console.log((expected+2) + ' tests passed.')
  }
}
