var request = request = require('../index')
  , assert = require('assert')
  ;
 

// Test adding a querystring
var req1 = request.get({ uri: 'http://www.google.com', qs: { q : 'search' }})
setTimeout(function() {
	assert.equal('/?q=search', req1.path)
}, 1)

// Test replacing a querystring value
var req2 = request.get({ uri: 'http://www.google.com?q=abc', qs: { q : 'search' }})
setTimeout(function() {
	assert.equal('/?q=search', req2.path)
}, 1)

// Test appending a querystring value to the ones present in the uri
var req3 = request.get({ uri: 'http://www.google.com?x=y', qs: { q : 'search' }})
setTimeout(function() {
	assert.equal('/?x=y&q=search', req3.path)
}, 1)

// Test leaving a querystring alone
var req4 = request.get({ uri: 'http://www.google.com?x=y'})
setTimeout(function() {
	assert.equal('/?x=y', req4.path)
}, 1)

// Test giving empty qs property
var req5 = request.get({ uri: 'http://www.google.com', qs: {}})
setTimeout(function(){
	assert.equal('/', req5.path)
}, 1)
