var request = require('../index')
  , assert = require('assert')
  ;

request.get({
  uri: 'http://www.google.com', localAddress: '1.2.3.4' // some invalid address
}, function(err, res) {
  assert(!res) // asserting that no response received
})

request.get({
  uri: 'http://www.google.com', localAddress: '127.0.0.1'
}, function(err, res) {
  assert(!res) // asserting that no response received
})
