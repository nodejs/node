var createServer = require('http').createServer
  , request = require('../index')
  , hawk = require('hawk')
  , assert = require('assert')
  ;

var server = createServer(function (req, resp) {
  
  var getCred = function (id, callback) {
    assert.equal(id, 'dh37fgj492je')
    var credentials = 
      { key: 'werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn'
      , algorithm: 'sha256'
      , user: 'Steve'
      }
    return callback(null, credentials)
  }

  hawk.server.authenticate(req, getCred, {}, function (err, credentials, attributes) {
    resp.writeHead(!err ? 200 : 401, { 'Content-Type': 'text/plain' })
    resp.end(!err ? 'Hello ' + credentials.user : 'Shoosh!')
  })
  
})

server.listen(8080, function () {
  var creds = {key: 'werxhqb98rpaxn39848xrunpaw3489ruxnpa98w4rxn', algorithm: 'sha256', id:'dh37fgj492je'}
  request('http://localhost:8080', {hawk:{credentials:creds}}, function (e, r, b) {
    assert.equal(200, r.statusCode)
    assert.equal(b, 'Hello Steve')
    server.close()
  })
})