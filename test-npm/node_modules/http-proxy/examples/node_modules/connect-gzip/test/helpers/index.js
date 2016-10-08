var assert = require('assert'),
    should = require('should'),
    spawn = require('child_process').spawn;

function wrapTest(func, numCallbacks) {
  numCallbacks = numCallbacks || 1;
  return function(beforeExit) {
    var n = 0;
    function done() { n++; }
    func(done);
    beforeExit(function() {
      n.should.equal(numCallbacks);
    });
  }
}

exports.testUncompressed = function(app, url, headers, resBody, resType, method) {
  return wrapTest(function(done) {
    assert.response(app, {
        url: url,
        method: method ? method : 'GET',
        headers: headers
      }, {
        status: 200,
        body: resBody,
        headers: { 'Content-Type': resType }
      }, function(res) {
        res.headers.should.not.have.property('content-encoding');
        done();
      }
    );
  });
}

exports.testCompressed = function(app, url, headers, resBody, resType, method) {
  return wrapTest(function(done) {
    assert.response(app, {
        url: url,
        method: method ? method : 'GET',
        headers: headers,
        encoding: 'binary'
      }, {
        status: 200,
        headers: {
          'Content-Type': resType,
          'Content-Encoding': 'gzip',
          'Vary': 'Accept-Encoding'
        }
      }, function(res) {
        res.body.should.not.equal(resBody);
        gunzip(res.body, function(err, body) {
          body.should.equal(resBody);
          done();
        });
      }
    );
  });
}

exports.testRedirect = function(app, url, headers, location) {
  return wrapTest(function(done) {
    assert.response(app, {
        url: url,
        headers: headers
      }, {
        status: 301,
        headers: {
          'Location': location
        }
      }, done
    );
  });
}

exports.testMaxAge = function(app, url, headers, maxAge) {
  return wrapTest(function(done) {
    assert.response(app, {
        url: url,
        headers: headers
      }, {
        status: 200,
        headers: {
          'Cache-Control': 'public, max-age=' + Math.floor(maxAge / 1000)
        }
      }, done
    );
  });
}

function gunzip(data, callback) {
  var process = spawn('gunzip', ['-c']),
      out = '',
      err = '';
  process.stdout.on('data', function(data) {
    out += data;
  });
  process.stderr.on('data', function(data) {
    err += data;
  });
  process.on('exit', function(code) {
    if (callback) callback(err, out);
  });
  process.stdin.end(data, 'binary');
}
