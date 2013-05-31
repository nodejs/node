var server = require('./server')
  , assert = require('assert')
  , request = require('../main.js')
  , Cookie = require('../vendor/cookie')
  , Jar = require('../vendor/cookie/jar')

var s = server.createServer()

s.listen(s.port, function () {
  var server = 'http://localhost:' + s.port;
  var hits = {}
  var passed = 0;

  bouncer(301, 'temp')
  bouncer(302, 'perm')
  bouncer(302, 'nope')

  function bouncer(code, label) {
    var landing = label+'_landing';

    s.on('/'+label, function (req, res) {
      hits[label] = true;
      res.writeHead(code, {
        'location':server + '/'+landing,
        'set-cookie': 'ham=eggs'
      })
      res.end()
    })

    s.on('/'+landing, function (req, res) {
      if (req.method !== 'GET') { // We should only accept GET redirects
        console.error("Got a non-GET request to the redirect destination URL");
        res.writeHead(400);
        res.end();
        return;
      }
      // Make sure the cookie doesn't get included twice, see #139:
      // Make sure cookies are set properly after redirect
      assert.equal(req.headers.cookie, 'foo=bar; quux=baz; ham=eggs');
      hits[landing] = true;
      res.writeHead(200)
      res.end(landing)
    })
  }

  // Permanent bounce
  var jar = new Jar()
  jar.add(new Cookie('quux=baz'))
  request({uri: server+'/perm', jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 200) throw new Error('Status is not 200: '+res.statusCode)
    assert.ok(hits.perm, 'Original request is to /perm')
    assert.ok(hits.perm_landing, 'Forward to permanent landing URL')
    assert.equal(body, 'perm_landing', 'Got permanent landing content')
    passed += 1
    done()
  })
  
  // Temporary bounce
  request({uri: server+'/temp', jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 200) throw new Error('Status is not 200: '+res.statusCode)
    assert.ok(hits.temp, 'Original request is to /temp')
    assert.ok(hits.temp_landing, 'Forward to temporary landing URL')
    assert.equal(body, 'temp_landing', 'Got temporary landing content')
    passed += 1
    done()
  })
  
  // Prevent bouncing.
  request({uri:server+'/nope', jar: jar, headers: {cookie: 'foo=bar'}, followRedirect:false}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 302) throw new Error('Status is not 302: '+res.statusCode)
    assert.ok(hits.nope, 'Original request to /nope')
    assert.ok(!hits.nope_landing, 'No chasing the redirect')
    assert.equal(res.statusCode, 302, 'Response is the bounce itself')
    passed += 1
    done()
  })
  
  // Should not follow post redirects by default
  request.post(server+'/temp', { jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 301) throw new Error('Status is not 301: '+res.statusCode)
    assert.ok(hits.temp, 'Original request is to /temp')
    assert.ok(!hits.temp_landing, 'No chasing the redirect when post')
    assert.equal(res.statusCode, 301, 'Response is the bounce itself')
    passed += 1
    done()
  })
  
  // Should follow post redirects when followAllRedirects true
  request.post({uri:server+'/temp', followAllRedirects:true, jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 200) throw new Error('Status is not 200: '+res.statusCode)
    assert.ok(hits.temp, 'Original request is to /temp')
    assert.ok(hits.temp_landing, 'Forward to temporary landing URL')
    assert.equal(body, 'temp_landing', 'Got temporary landing content')
    passed += 1
    done()
  })
  
  request.post({uri:server+'/temp', followAllRedirects:false, jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 301) throw new Error('Status is not 301: '+res.statusCode)
    assert.ok(hits.temp, 'Original request is to /temp')
    assert.ok(!hits.temp_landing, 'No chasing the redirect')
    assert.equal(res.statusCode, 301, 'Response is the bounce itself')
    passed += 1
    done()
  })

  // Should not follow delete redirects by default
  request.del(server+'/temp', { jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode < 301) throw new Error('Status is not a redirect.')
    assert.ok(hits.temp, 'Original request is to /temp')
    assert.ok(!hits.temp_landing, 'No chasing the redirect when delete')
    assert.equal(res.statusCode, 301, 'Response is the bounce itself')
    passed += 1
    done()
  })
  
  // Should not follow delete redirects even if followRedirect is set to true
  request.del(server+'/temp', { followRedirect: true, jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 301) throw new Error('Status is not 301: '+res.statusCode)
    assert.ok(hits.temp, 'Original request is to /temp')
    assert.ok(!hits.temp_landing, 'No chasing the redirect when delete')
    assert.equal(res.statusCode, 301, 'Response is the bounce itself')
    passed += 1
    done()
  })
  
  // Should follow delete redirects when followAllRedirects true
  request.del(server+'/temp', {followAllRedirects:true, jar: jar, headers: {cookie: 'foo=bar'}}, function (er, res, body) {
    if (er) throw er
    if (res.statusCode !== 200) throw new Error('Status is not 200: '+res.statusCode)
    assert.ok(hits.temp, 'Original request is to /temp')
    assert.ok(hits.temp_landing, 'Forward to temporary landing URL')
    assert.equal(body, 'temp_landing', 'Got temporary landing content')
    passed += 1
    done()
  })

  var reqs_done = 0;
  function done() {
    reqs_done += 1;
    if(reqs_done == 9) {
      console.log(passed + ' tests passed.')
      s.close()
    }
  }
})
