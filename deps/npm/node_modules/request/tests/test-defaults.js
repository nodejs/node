var server = require('./server')
  , assert = require('assert')
  , request = require('../main.js')
  ;

var s = server.createServer();

s.listen(s.port, function () {
  var counter = 0;
  s.on('/get', function (req, resp) {
    assert.equal(req.headers.foo, 'bar');
    assert.equal(req.method, 'GET')
    resp.writeHead(200, {'Content-Type': 'text/plain'});
    resp.end('TESTING!');
  });

  // test get(string, function)
  request.defaults({headers:{foo:"bar"}})(s.url + '/get', function (e, r, b){
    if (e) throw e;
    assert.deepEqual("TESTING!", b);
    counter += 1;
  });

  s.on('/post', function (req, resp) {
    assert.equal(req.headers.foo, 'bar');
    assert.equal(req.headers['content-type'], 'application/json');
    assert.equal(req.method, 'POST')
    resp.writeHead(200, {'Content-Type': 'application/json'});
    resp.end(JSON.stringify({foo:'bar'}));
  });

  // test post(string, object, function)
  request.defaults({headers:{foo:"bar"}}).post(s.url + '/post', {json: true}, function (e, r, b){
    if (e) throw e;
    assert.deepEqual('bar', b.foo);
    counter += 1;
  });

  s.on('/del', function (req, resp) {
    assert.equal(req.headers.foo, 'bar');
    assert.equal(req.method, 'DELETE')
    resp.writeHead(200, {'Content-Type': 'application/json'});
    resp.end(JSON.stringify({foo:'bar'}));
  });

  // test .del(string, function)
  request.defaults({headers:{foo:"bar"}, json:true}).del(s.url + '/del', function (e, r, b){
    if (e) throw e;
    assert.deepEqual('bar', b.foo);
    counter += 1;
  });

  s.on('/head', function (req, resp) {
    assert.equal(req.headers.foo, 'bar');
    assert.equal(req.method, 'HEAD')
    resp.writeHead(200, {'Content-Type': 'text/plain'});
    resp.end();
  });

  // test head.(object, function)
  request.defaults({headers:{foo:"bar"}}).head({uri: s.url + '/head'}, function (e, r, b){
    if (e) throw e;
    counter += 1;
  });

  s.on('/get_custom', function(req, resp) {
    assert.equal(req.headers.foo, 'bar');
    assert.equal(req.headers.x, 'y');
    resp.writeHead(200, {'Content-Type': 'text/plain'});
    resp.end();
  });

  // test custom request handler function
  var defaultRequest = request.defaults({
    headers:{foo:"bar"}
    , body: 'TESTING!'
  }, function(uri, options, callback) {
    var params = request.initParams(uri, options, callback);
    options = params.options;
    options.headers.x = 'y';

    return request(params.uri, params.options, params.callback);
  });

  var msg = 'defaults test failed. head request should throw earlier';
  assert.throws(function() {
    defaultRequest.head(s.url + '/get_custom', function(e, r, b) {
      throw new Error(msg);
    });
    counter+=1;
  }, msg);

  defaultRequest.get(s.url + '/get_custom', function(e, r, b) {
    if(e) throw e;
    counter += 1;
    console.log(counter.toString() + " tests passed.");
    s.close();
  });
})
