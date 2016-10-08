var connect = require('connect'),
    assert  = require('assert'),
    jsonp   = require('../lib/connect-jsonp');

var expectedRes = "{data: 'data'}";
var expectedPaddedRes = "cb(" + expectedRes +")";

app = connect(
  jsonp(),
  function(req, res) {
    res.writeHead(200, {});
    res.end(expectedRes);   
  }
);

filteringApp = connect(
  jsonp(true),
  function(req, res) {
    res.writeHead(200, {});
    res.end(expectedRes);   
  }
);
  
module.exports = {
  'test not a padding request': function() {
    assert.response(app, { url: '/' }, { 
      body: expectedRes, 
      status: 200
    });
  },

  'test not a get request': function() {
    assert.response(app, {url: '/?callback=cb', method: 'POST'}, {
      body: 'cb({"error":"method not allowed","description":"with callback only GET allowed"})',
      status: 400,
      headers: {
        'Content-Type': 'application/javascript'
      }
    });     
  },

  'test query string': function() {
    assert.response(app, {url: '/test?callback=cb&test=test', method: 'GET'}, {
      body: expectedPaddedRes,
      status: 200,
      headers: {
        'Content-Type': 'application/javascript',
        'Content-Length': expectedPaddedRes.length
      }
    }); 
  },

  'test filtering callback param from downstream middleware': function() {
    assert.response(filteringApp, {url: '/test?callback=cb&test=test', method: 'GET'}, {
      body: expectedPaddedRes,
      status: 200,
      headers: {
        'Content-Type': 'application/javascript',
        'Content-Length': expectedPaddedRes.length
      }
    });     
  }
};