var Cookie = require('../index')
  , Jar = Cookie.Jar
  , assert = require('assert');

function expires(ms) {
  return new Date(Date.now() + ms).toUTCString();
}

// test .get() expiration
(function() {
  var jar = new Jar;
  var cookie = new Cookie('sid=1234; path=/; expires=' + expires(1000));
  jar.add(cookie);
  setTimeout(function(){
    var cookies = jar.get({ url: 'http://foo.com/foo' });
    assert.equal(cookies.length, 1);
    assert.equal(cookies[0], cookie);
    setTimeout(function(){
      var cookies = jar.get({ url: 'http://foo.com/foo' });
      assert.equal(cookies.length, 0);
    }, 1000);
  }, 5);
})();

// test .get() path support
(function() {
  var jar = new Jar;
  var a = new Cookie('sid=1234; path=/');
  var b = new Cookie('sid=1111; path=/foo/bar');
  var c = new Cookie('sid=2222; path=/');
  jar.add(a);
  jar.add(b);
  jar.add(c);

  // should remove the duplicates
  assert.equal(jar.cookies.length, 2);

  // same name, same path, latter prevails
  var cookies = jar.get({ url: 'http://foo.com/' });
  assert.equal(cookies.length, 1);
  assert.equal(cookies[0], c);

  // same name, diff path, path specifity prevails, latter prevails
  var cookies = jar.get({ url: 'http://foo.com/foo/bar' });
  assert.equal(cookies.length, 1);
  assert.equal(cookies[0], b);

  var jar = new Jar;
  var a = new Cookie('sid=1111; path=/foo/bar');
  var b = new Cookie('sid=1234; path=/');
  jar.add(a);
  jar.add(b);

  var cookies = jar.get({ url: 'http://foo.com/foo/bar' });
  assert.equal(cookies.length, 1);
  assert.equal(cookies[0], a);

  var cookies = jar.get({ url: 'http://foo.com/' });
  assert.equal(cookies.length, 1);
  assert.equal(cookies[0], b);

  var jar = new Jar;
  var a = new Cookie('sid=1111; path=/foo/bar');
  var b = new Cookie('sid=3333; path=/foo/bar');
  var c = new Cookie('pid=3333; path=/foo/bar');
  var d = new Cookie('sid=2222; path=/foo/');
  var e = new Cookie('sid=1234; path=/');
  jar.add(a);
  jar.add(b);
  jar.add(c);
  jar.add(d);
  jar.add(e);

  var cookies = jar.get({ url: 'http://foo.com/foo/bar' });
  assert.equal(cookies.length, 2);
  assert.equal(cookies[0], b);
  assert.equal(cookies[1], c);

  var cookies = jar.get({ url: 'http://foo.com/foo/' });
  assert.equal(cookies.length, 1);
  assert.equal(cookies[0], d);

  var cookies = jar.get({ url: 'http://foo.com/' });
  assert.equal(cookies.length, 1);
  assert.equal(cookies[0], e);
})();

setTimeout(function() {
  console.log('All tests passed');
}, 1200);
