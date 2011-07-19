// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var url = require('url'),
    util = require('util');

// URLs to parse, and expected data
// { url : parsed }
var parseTests = {
  '//some_path' : {
    'href': '//some_path',
    'pathname': '//some_path'
  },
  'HTTP://www.example.com/' : {
    'href': 'http://www.example.com/',
    'protocol': 'http:',
    'host': 'www.example.com',
    'hostname': 'www.example.com',
    'pathname': '/'
  },
  'http://www.ExAmPlE.com/' : {
    'href': 'http://www.example.com/',
    'protocol': 'http:',
    'host': 'www.example.com',
    'hostname': 'www.example.com',
    'pathname': '/'

  },
  'http://user:pw@www.ExAmPlE.com/' : {
    'href': 'http://user:pw@www.example.com/',
    'protocol': 'http:',
    'auth': 'user:pw',
    'host': 'user:pw@www.example.com',
    'hostname': 'www.example.com',
    'pathname': '/'

  },
  'http://USER:PW@www.ExAmPlE.com/' : {
    'href': 'http://USER:PW@www.example.com/',
    'protocol': 'http:',
    'auth': 'USER:PW',
    'host': 'USER:PW@www.example.com',
    'hostname': 'www.example.com',
    'pathname': '/'
  },
  'http://x.com/path?that\'s#all, folks' : {
    'href': 'http://x.com/path?that%27s#all,',
    'protocol': 'http:',
    'host': 'x.com',
    'hostname': 'x.com',
    'search': '?that%27s',
    'query': 'that%27s',
    'pathname': '/path',
    'hash': '#all,'
  },
  'HTTP://X.COM/Y' : {
    'href': 'http://x.com/Y',
    'protocol': 'http:',
    'host': 'x.com',
    'hostname': 'x.com',
    'pathname': '/Y',
  },
  // an unexpected invalid char in the hostname.
  'HtTp://x.y.cOm*a/b/c?d=e#f g<h>i' : {
    'href': 'http://x.y.com/*a/b/c?d=e#f',
    'protocol': 'http:',
    'host': 'x.y.com',
    'hostname': 'x.y.com',
    'pathname': '/*a/b/c',
    'search': '?d=e',
    'query': 'd=e',
    'hash': '#f'
  },
  // make sure that we don't accidentally lcast the path parts.
  'HtTp://x.y.cOm*A/b/c?d=e#f g<h>i' : {
    'href': 'http://x.y.com/*A/b/c?d=e#f',
    'protocol': 'http:',
    'host': 'x.y.com',
    'hostname': 'x.y.com',
    'pathname': '/*A/b/c',
    'search': '?d=e',
    'query': 'd=e',
    'hash': '#f'
  },
  'http://x...y...#p': {
    'href': 'http://x...y.../#p',
    'protocol': 'http:',
    'host': 'x...y...',
    'hostname': 'x...y...',
    'hash': '#p',
    'pathname': '/'
  },
  'http://x/p/"quoted"': {
    'href': 'http://x/p/',
    'protocol':'http:',
    'host': 'x',
    'hostname': 'x',
    'pathname': '/p/'
  },
  '<http://goo.corn/bread> Is a URL!': {
    'href': 'http://goo.corn/bread',
    'protocol': 'http:',
    'host': 'goo.corn',
    'hostname': 'goo.corn',
    'pathname': '/bread'
  },
  'http://www.narwhaljs.org/blog/categories?id=news' : {
    'href': 'http://www.narwhaljs.org/blog/categories?id=news',
    'protocol': 'http:',
    'host': 'www.narwhaljs.org',
    'hostname': 'www.narwhaljs.org',
    'search': '?id=news',
    'query': 'id=news',
    'pathname': '/blog/categories'
  },
  'http://mt0.google.com/vt/lyrs=m@114&hl=en&src=api&x=2&y=2&z=3&s=' : {
    'href': 'http://mt0.google.com/vt/lyrs=m@114&hl=en&src=api&x=2&y=2&z=3&s=',
    'protocol': 'http:',
    'host': 'mt0.google.com',
    'hostname': 'mt0.google.com',
    'pathname': '/vt/lyrs=m@114&hl=en&src=api&x=2&y=2&z=3&s='
  },
  'http://mt0.google.com/vt/lyrs=m@114???&hl=en&src=api&x=2&y=2&z=3&s=' : {
    'href': 'http://mt0.google.com/vt/lyrs=m@114???&hl=en&src=api' +
        '&x=2&y=2&z=3&s=',
    'protocol': 'http:',
    'host': 'mt0.google.com',
    'hostname': 'mt0.google.com',
    'search': '???&hl=en&src=api&x=2&y=2&z=3&s=',
    'query': '??&hl=en&src=api&x=2&y=2&z=3&s=',
    'pathname': '/vt/lyrs=m@114'
  },
  'http://user:pass@mt0.google.com/vt/lyrs=m@114???&hl=en&src=api&x=2&y=2&z=3&s=':
      {
        'href': 'http://user:pass@mt0.google.com/vt/lyrs=m@114???' +
            '&hl=en&src=api&x=2&y=2&z=3&s=',
        'protocol': 'http:',
        'host': 'user:pass@mt0.google.com',
        'auth': 'user:pass',
        'hostname': 'mt0.google.com',
        'search': '???&hl=en&src=api&x=2&y=2&z=3&s=',
        'query': '??&hl=en&src=api&x=2&y=2&z=3&s=',
        'pathname': '/vt/lyrs=m@114'
      },
  'file:///etc/passwd' : {
    'href': 'file:///etc/passwd',
    'protocol': 'file:',
    'pathname': '/etc/passwd',
    'hostname': ''
  },
  'file://localhost/etc/passwd' : {
    'href': 'file://localhost/etc/passwd',
    'protocol': 'file:',
    'pathname': '/etc/passwd',
    'hostname': 'localhost'
  },
  'file://foo/etc/passwd' : {
    'href': 'file://foo/etc/passwd',
    'protocol': 'file:',
    'pathname': '/etc/passwd',
    'hostname': 'foo'
  },
  'file:///etc/node/' : {
    'href': 'file:///etc/node/',
    'protocol': 'file:',
    'pathname': '/etc/node/',
    'hostname': ''
  },
  'file://localhost/etc/node/' : {
    'href': 'file://localhost/etc/node/',
    'protocol': 'file:',
    'pathname': '/etc/node/',
    'hostname': 'localhost'
  },
  'file://foo/etc/node/' : {
    'href': 'file://foo/etc/node/',
    'protocol': 'file:',
    'pathname': '/etc/node/',
    'hostname': 'foo'
  },
  'http:/baz/../foo/bar' : {
    'href': 'http:/baz/../foo/bar',
    'protocol': 'http:',
    'pathname': '/baz/../foo/bar'
  },
  'http://user:pass@example.com:8000/foo/bar?baz=quux#frag' : {
    'href': 'http://user:pass@example.com:8000/foo/bar?baz=quux#frag',
    'protocol': 'http:',
    'host': 'user:pass@example.com:8000',
    'auth': 'user:pass',
    'port': '8000',
    'hostname': 'example.com',
    'hash': '#frag',
    'search': '?baz=quux',
    'query': 'baz=quux',
    'pathname': '/foo/bar'
  },
  '//user:pass@example.com:8000/foo/bar?baz=quux#frag' : {
    'href': '//user:pass@example.com:8000/foo/bar?baz=quux#frag',
    'host': 'user:pass@example.com:8000',
    'auth': 'user:pass',
    'port': '8000',
    'hostname': 'example.com',
    'hash': '#frag',
    'search': '?baz=quux',
    'query': 'baz=quux',
    'pathname': '/foo/bar'
  },
  '/foo/bar?baz=quux#frag' : {
    'href': '/foo/bar?baz=quux#frag',
    'hash': '#frag',
    'search': '?baz=quux',
    'query': 'baz=quux',
    'pathname': '/foo/bar'
  },
  'http:/foo/bar?baz=quux#frag' : {
    'href': 'http:/foo/bar?baz=quux#frag',
    'protocol': 'http:',
    'hash': '#frag',
    'search': '?baz=quux',
    'query': 'baz=quux',
    'pathname': '/foo/bar'
  },
  'mailto:foo@bar.com?subject=hello' : {
    'href': 'mailto:foo@bar.com?subject=hello',
    'protocol': 'mailto:',
    'host': 'foo@bar.com',
    'auth' : 'foo',
    'hostname' : 'bar.com',
    'search': '?subject=hello',
    'query': 'subject=hello'
  },
  'javascript:alert(\'hello\');' : {
    'href': 'javascript:alert(\'hello\');',
    'protocol': 'javascript:',
    'pathname': 'alert(\'hello\');'
  },
  'xmpp:isaacschlueter@jabber.org' : {
    'href': 'xmpp:isaacschlueter@jabber.org',
    'protocol': 'xmpp:',
    'host': 'isaacschlueter@jabber.org',
    'auth': 'isaacschlueter',
    'hostname': 'jabber.org'
  },
  'http://atpass:foo%40bar@127.0.0.1:8080/path?search=foo#bar' : {
    'href' : 'http://atpass:foo%40bar@127.0.0.1:8080/path?search=foo#bar',
    'protocol' : 'http:',
    'host' : 'atpass:foo%40bar@127.0.0.1:8080',
    'auth' : 'atpass:foo%40bar',
    'hostname' : '127.0.0.1',
    'port' : '8080',
    'pathname': '/path',
    'search' : '?search=foo',
    'query' : 'search=foo',
    'hash' : '#bar'
  },
  'http://bucket_name.s3.amazonaws.com/image.jpg': {
    protocol: 'http:',
    slashes: true,
    host: 'bucket_name.s3.amazonaws.com',
    hostname: 'bucket_name.s3.amazonaws.com',
    pathname: '/image.jpg',
    href: 'http://bucket_name.s3.amazonaws.com/image.jpg'
  }
};

for (var u in parseTests) {
  var actual = url.parse(u),
      expected = parseTests[u];
  for (var i in expected) {
    var e = JSON.stringify(expected[i]),
        a = JSON.stringify(actual[i]);
    assert.equal(e, a,
                 'parse(' + u + ').' + i + ' == ' + e + '\nactual: ' + a);
  }

  var expected = parseTests[u].href,
      actual = url.format(parseTests[u]);

  assert.equal(expected, actual,
               'format(' + u + ') == ' + u + '\nactual:' + actual);
}

var parseTestsWithQueryString = {
  '/foo/bar?baz=quux#frag' : {
    'href': '/foo/bar?baz=quux#frag',
    'hash': '#frag',
    'search': '?baz=quux',
    'query': {
      'baz': 'quux'
    },
    'pathname': '/foo/bar'
  },
  'http://example.com' : {
    'href': 'http://example.com/',
    'protocol': 'http:',
    'slashes': true,
    'host': 'example.com',
    'hostname': 'example.com',
    'query': {},
    'pathname': '/'
  }
};
for (var u in parseTestsWithQueryString) {
  var actual = url.parse(u, true);
  var expected = parseTestsWithQueryString[u];
  for (var i in expected) {
    var e = JSON.stringify(expected[i]),
        a = JSON.stringify(actual[i]);
    assert.equal(e, a,
                 'parse(' + u + ').' + i + ' == ' + e + '\nactual: ' + a);
  }
}

// some extra formatting tests, just to verify
// that it'll format slightly wonky content to a valid url.
var formatTests = {
  'http://example.com?' : {
    'href': 'http://example.com/?',
    'protocol': 'http:',
    'slashes': true,
    'host': 'example.com',
    'hostname': 'example.com',
    'search': '?',
    'query': {},
    'pathname': '/'
  },
  'http://example.com?foo=bar#frag' : {
    'href': 'http://example.com/?foo=bar#frag',
    'protocol': 'http:',
    'host': 'example.com',
    'hostname': 'example.com',
    'hash': '#frag',
    'search': '?foo=bar',
    'query': 'foo=bar',
    'pathname': '/'
  },
  'http://example.com?foo=@bar#frag' : {
    'href': 'http://example.com/?foo=@bar#frag',
    'protocol': 'http:',
    'host': 'example.com',
    'hostname': 'example.com',
    'hash': '#frag',
    'search': '?foo=@bar',
    'query': 'foo=@bar',
    'pathname': '/'
  },
  'http://example.com?foo=/bar/#frag' : {
    'href': 'http://example.com/?foo=/bar/#frag',
    'protocol': 'http:',
    'host': 'example.com',
    'hostname': 'example.com',
    'hash': '#frag',
    'search': '?foo=/bar/',
    'query': 'foo=/bar/',
    'pathname': '/'
  },
  'http://example.com?foo=?bar/#frag' : {
    'href': 'http://example.com/?foo=?bar/#frag',
    'protocol': 'http:',
    'host': 'example.com',
    'hostname': 'example.com',
    'hash': '#frag',
    'search': '?foo=?bar/',
    'query': 'foo=?bar/',
    'pathname': '/'
  },
  'http://example.com#frag=?bar/#frag' : {
    'href': 'http://example.com/#frag=?bar/#frag',
    'protocol': 'http:',
    'host': 'example.com',
    'hostname': 'example.com',
    'hash': '#frag=?bar/#frag',
    'pathname': '/'
  },
  'http://google.com" onload="alert(42)/' : {
    'href': 'http://google.com/',
    'protocol': 'http:',
    'host': 'google.com',
    'pathname': '/'
  },
  'http://a.com/a/b/c?s#h' : {
    'href': 'http://a.com/a/b/c?s#h',
    'protocol': 'http',
    'host': 'a.com',
    'pathname': 'a/b/c',
    'hash': 'h',
    'search': 's'
  },
  'xmpp:isaacschlueter@jabber.org' : {
    'href': 'xmpp:isaacschlueter@jabber.org',
    'protocol': 'xmpp:',
    'host': 'isaacschlueter@jabber.org',
    'auth': 'isaacschlueter',
    'hostname': 'jabber.org'
  },
  'http://atpass:foo%40bar@127.0.0.1/' : {
    'href': 'http://atpass:foo%40bar@127.0.0.1/',
    'auth': 'atpass:foo@bar',
    'hostname': '127.0.0.1',
    'protocol': 'http:',
    'pathname': '/'
  },
  'http://atslash%2F%40:%2F%40@foo/' : {
    'href': 'http://atslash%2F%40:%2F%40@foo/',
    'auth': 'atslash/@:/@',
    'hostname': 'foo',
    'protocol': 'http:',
    'pathname': '/'
  }
};
for (var u in formatTests) {
  var expect = formatTests[u].href;
  delete formatTests[u].href;
  var actual = url.format(u);
  var actualObj = url.format(formatTests[u]);
  assert.equal(actual, expect,
               'wonky format(' + u + ') == ' + expect +
               '\nactual:' + actual);
  assert.equal(actualObj, expect,
               'wonky format(' + JSON.stringify(formatTests[u]) +
               ') == ' + expect +
               '\nactual: ' + actualObj);
}

/*
 [from, path, expected]
*/
var relativeTests = [
  ['/foo/bar/baz', 'quux', '/foo/bar/quux'],
  ['/foo/bar/baz', 'quux/asdf', '/foo/bar/quux/asdf'],
  ['/foo/bar/baz', 'quux/baz', '/foo/bar/quux/baz'],
  ['/foo/bar/baz', '../quux/baz', '/foo/quux/baz'],
  ['/foo/bar/baz', '/bar', '/bar'],
  ['/foo/bar/baz/', 'quux', '/foo/bar/baz/quux'],
  ['/foo/bar/baz/', 'quux/baz', '/foo/bar/baz/quux/baz'],
  ['/foo/bar/baz', '../../../../../../../../quux/baz', '/quux/baz'],
  ['/foo/bar/baz', '../../../../../../../quux/baz', '/quux/baz'],
  ['foo/bar', '../../../baz', '../../baz'],
  ['foo/bar/', '../../../baz', '../baz'],
  ['http://example.com/b//c//d;p?q#blarg', 'https:#hash2', 'https:///#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'https:/p/a/t/h?s#hash2',
   'https://p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'https://u:p@h.com/p/a/t/h?s#hash2',
   'https://u:p@h.com/p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'https:/a/b/c/d',
   'https://a/b/c/d'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http:#hash2',
   'http://example.com/b//c//d;p?q#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http:/p/a/t/h?s#hash2',
   'http://example.com/p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http://u:p@h.com/p/a/t/h?s#hash2',
   'http://u:p@h.com/p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http:/a/b/c/d',
   'http://example.com/a/b/c/d'],
  ['/foo/bar/baz', '/../etc/passwd', '/etc/passwd']
];
relativeTests.forEach(function(relativeTest) {
  var a = url.resolve(relativeTest[0], relativeTest[1]),
      e = relativeTest[2];
  assert.equal(e, a,
               'resolve(' + [relativeTest[0], relativeTest[1]] + ') == ' + e +
               '\n  actual=' + a);
});


//
// Tests below taken from Chiron
// http://code.google.com/p/chironjs/source/browse/trunk/src/test/http/url.js
//
// Copyright (c) 2002-2008 Kris Kowal <http://cixar.com/~kris.kowal>
// used with permission under MIT License
//
// Changes marked with @isaacs

var bases = [
  'http://a/b/c/d;p?q',
  'http://a/b/c/d;p?q=1/2',
  'http://a/b/c/d;p=1/2?q',
  'fred:///s//a/b/c',
  'http:///s//a/b/c'
];

//[to, from, result]
var relativeTests2 = [
  // http://lists.w3.org/Archives/Public/uri/2004Feb/0114.html
  ['../c', 'foo:a/b', 'foo:c'],
  ['foo:.', 'foo:a', 'foo:'],
  ['/foo/../../../bar', 'zz:abc', 'zz:/bar'],
  ['/foo/../bar', 'zz:abc', 'zz:/bar'],
  // @isaacs Disagree. Not how web browsers resolve this.
  ['foo/../../../bar', 'zz:abc', 'zz:bar'],
  // ['foo/../../../bar',  'zz:abc', 'zz:../../bar'], // @isaacs Added
  ['foo/../bar', 'zz:abc', 'zz:bar'],
  ['zz:.', 'zz:abc', 'zz:'],
  ['/.', bases[0], 'http://a/'],
  ['/.foo', bases[0], 'http://a/.foo'],
  ['.foo', bases[0], 'http://a/b/c/.foo'],

  // http://gbiv.com/protocols/uri/test/rel_examples1.html
  // examples from RFC 2396
  ['g:h', bases[0], 'g:h'],
  ['g', bases[0], 'http://a/b/c/g'],
  ['./g', bases[0], 'http://a/b/c/g'],
  ['g/', bases[0], 'http://a/b/c/g/'],
  ['/g', bases[0], 'http://a/g'],
  ['//g', bases[0], 'http://g'],
  // changed with RFC 2396bis
  //('?y', bases[0], 'http://a/b/c/d;p?y'],
  ['?y', bases[0], 'http://a/b/c/d;p?y'],
  ['g?y', bases[0], 'http://a/b/c/g?y'],
  // changed with RFC 2396bis
  //('#s', bases[0], CURRENT_DOC_URI + '#s'],
  ['#s', bases[0], 'http://a/b/c/d;p?q#s'],
  ['g#s', bases[0], 'http://a/b/c/g#s'],
  ['g?y#s', bases[0], 'http://a/b/c/g?y#s'],
  [';x', bases[0], 'http://a/b/c/;x'],
  ['g;x', bases[0], 'http://a/b/c/g;x'],
  ['g;x?y#s' , bases[0], 'http://a/b/c/g;x?y#s'],
  // changed with RFC 2396bis
  //('', bases[0], CURRENT_DOC_URI],
  ['', bases[0], 'http://a/b/c/d;p?q'],
  ['.', bases[0], 'http://a/b/c/'],
  ['./', bases[0], 'http://a/b/c/'],
  ['..', bases[0], 'http://a/b/'],
  ['../', bases[0], 'http://a/b/'],
  ['../g', bases[0], 'http://a/b/g'],
  ['../..', bases[0], 'http://a/'],
  ['../../', bases[0], 'http://a/'],
  ['../../g' , bases[0], 'http://a/g'],
  ['../../../g', bases[0], ('http://a/../g', 'http://a/g')],
  ['../../../../g', bases[0], ('http://a/../../g', 'http://a/g')],
  // changed with RFC 2396bis
  //('/./g', bases[0], 'http://a/./g'],
  ['/./g', bases[0], 'http://a/g'],
  // changed with RFC 2396bis
  //('/../g', bases[0], 'http://a/../g'],
  ['/../g', bases[0], 'http://a/g'],
  ['g.', bases[0], 'http://a/b/c/g.'],
  ['.g', bases[0], 'http://a/b/c/.g'],
  ['g..', bases[0], 'http://a/b/c/g..'],
  ['..g', bases[0], 'http://a/b/c/..g'],
  ['./../g', bases[0], 'http://a/b/g'],
  ['./g/.', bases[0], 'http://a/b/c/g/'],
  ['g/./h', bases[0], 'http://a/b/c/g/h'],
  ['g/../h', bases[0], 'http://a/b/c/h'],
  ['g;x=1/./y', bases[0], 'http://a/b/c/g;x=1/y'],
  ['g;x=1/../y', bases[0], 'http://a/b/c/y'],
  ['g?y/./x', bases[0], 'http://a/b/c/g?y/./x'],
  ['g?y/../x', bases[0], 'http://a/b/c/g?y/../x'],
  ['g#s/./x', bases[0], 'http://a/b/c/g#s/./x'],
  ['g#s/../x', bases[0], 'http://a/b/c/g#s/../x'],
  ['http:g', bases[0], ('http:g', 'http://a/b/c/g')],
  ['http:', bases[0], ('http:', bases[0])],
  // not sure where this one originated
  ['/a/b/c/./../../g', bases[0], 'http://a/a/g'],

  // http://gbiv.com/protocols/uri/test/rel_examples2.html
  // slashes in base URI's query args
  ['g', bases[1], 'http://a/b/c/g'],
  ['./g', bases[1], 'http://a/b/c/g'],
  ['g/', bases[1], 'http://a/b/c/g/'],
  ['/g', bases[1], 'http://a/g'],
  ['//g', bases[1], 'http://g'],
  // changed in RFC 2396bis
  //('?y', bases[1], 'http://a/b/c/?y'],
  ['?y', bases[1], 'http://a/b/c/d;p?y'],
  ['g?y', bases[1], 'http://a/b/c/g?y'],
  ['g?y/./x' , bases[1], 'http://a/b/c/g?y/./x'],
  ['g?y/../x', bases[1], 'http://a/b/c/g?y/../x'],
  ['g#s', bases[1], 'http://a/b/c/g#s'],
  ['g#s/./x' , bases[1], 'http://a/b/c/g#s/./x'],
  ['g#s/../x', bases[1], 'http://a/b/c/g#s/../x'],
  ['./', bases[1], 'http://a/b/c/'],
  ['../', bases[1], 'http://a/b/'],
  ['../g', bases[1], 'http://a/b/g'],
  ['../../', bases[1], 'http://a/'],
  ['../../g' , bases[1], 'http://a/g'],

  // http://gbiv.com/protocols/uri/test/rel_examples3.html
  // slashes in path params
  // all of these changed in RFC 2396bis
  ['g', bases[2], 'http://a/b/c/d;p=1/g'],
  ['./g', bases[2], 'http://a/b/c/d;p=1/g'],
  ['g/', bases[2], 'http://a/b/c/d;p=1/g/'],
  ['g?y', bases[2], 'http://a/b/c/d;p=1/g?y'],
  [';x', bases[2], 'http://a/b/c/d;p=1/;x'],
  ['g;x', bases[2], 'http://a/b/c/d;p=1/g;x'],
  ['g;x=1/./y', bases[2], 'http://a/b/c/d;p=1/g;x=1/y'],
  ['g;x=1/../y', bases[2], 'http://a/b/c/d;p=1/y'],
  ['./', bases[2], 'http://a/b/c/d;p=1/'],
  ['../', bases[2], 'http://a/b/c/'],
  ['../g', bases[2], 'http://a/b/c/g'],
  ['../../', bases[2], 'http://a/b/'],
  ['../../g' , bases[2], 'http://a/b/g'],

  // http://gbiv.com/protocols/uri/test/rel_examples4.html
  // double and triple slash, unknown scheme
  ['g:h', bases[3], 'g:h'],
  ['g', bases[3], 'fred:///s//a/b/g'],
  ['./g', bases[3], 'fred:///s//a/b/g'],
  ['g/', bases[3], 'fred:///s//a/b/g/'],
  ['/g', bases[3], 'fred:///g'],  // may change to fred:///s//a/g
  ['//g', bases[3], 'fred://g'],   // may change to fred:///s//g
  ['//g/x', bases[3], 'fred://g/x'], // may change to fred:///s//g/x
  ['///g', bases[3], 'fred:///g'],
  ['./', bases[3], 'fred:///s//a/b/'],
  ['../', bases[3], 'fred:///s//a/'],
  ['../g', bases[3], 'fred:///s//a/g'],

  ['../../', bases[3], 'fred:///s//'],
  ['../../g' , bases[3], 'fred:///s//g'],
  ['../../../g', bases[3], 'fred:///s/g'],
  // may change to fred:///s//a/../../../g
  ['../../../../g', bases[3], 'fred:///g'],

  // http://gbiv.com/protocols/uri/test/rel_examples5.html
  // double and triple slash, well-known scheme
  ['g:h', bases[4], 'g:h'],
  ['g', bases[4], 'http:///s//a/b/g'],
  ['./g', bases[4], 'http:///s//a/b/g'],
  ['g/', bases[4], 'http:///s//a/b/g/'],
  ['/g', bases[4], 'http:///g'],  // may change to http:///s//a/g
  ['//g', bases[4], 'http://g'],   // may change to http:///s//g
  ['//g/x', bases[4], 'http://g/x'], // may change to http:///s//g/x
  ['///g', bases[4], 'http:///g'],
  ['./', bases[4], 'http:///s//a/b/'],
  ['../', bases[4], 'http:///s//a/'],
  ['../g', bases[4], 'http:///s//a/g'],
  ['../../', bases[4], 'http:///s//'],
  ['../../g' , bases[4], 'http:///s//g'],
  // may change to http:///s//a/../../g
  ['../../../g', bases[4], 'http:///s/g'],
  // may change to http:///s//a/../../../g
  ['../../../../g', bases[4], 'http:///g'],

  // from Dan Connelly's tests in http://www.w3.org/2000/10/swap/uripath.py
  ['bar:abc', 'foo:xyz', 'bar:abc'],
  ['../abc', 'http://example/x/y/z', 'http://example/x/abc'],
  ['http://example/x/abc', 'http://example2/x/y/z', 'http://example/x/abc'],
  ['../r', 'http://ex/x/y/z', 'http://ex/x/r'],
  ['q/r', 'http://ex/x/y', 'http://ex/x/q/r'],
  ['q/r#s', 'http://ex/x/y', 'http://ex/x/q/r#s'],
  ['q/r#s/t', 'http://ex/x/y', 'http://ex/x/q/r#s/t'],
  ['ftp://ex/x/q/r', 'http://ex/x/y', 'ftp://ex/x/q/r'],
  ['', 'http://ex/x/y', 'http://ex/x/y'],
  ['', 'http://ex/x/y/', 'http://ex/x/y/'],
  ['', 'http://ex/x/y/pdq', 'http://ex/x/y/pdq'],
  ['z/', 'http://ex/x/y/', 'http://ex/x/y/z/'],
  ['#Animal',
   'file:/swap/test/animal.rdf',
   'file:/swap/test/animal.rdf#Animal'],
  ['../abc', 'file:/e/x/y/z', 'file:/e/x/abc'],
  ['/example/x/abc', 'file:/example2/x/y/z', 'file:/example/x/abc'],
  ['../r', 'file:/ex/x/y/z', 'file:/ex/x/r'],
  ['/r', 'file:/ex/x/y/z', 'file:/r'],
  ['q/r', 'file:/ex/x/y', 'file:/ex/x/q/r'],
  ['q/r#s', 'file:/ex/x/y', 'file:/ex/x/q/r#s'],
  ['q/r#', 'file:/ex/x/y', 'file:/ex/x/q/r#'],
  ['q/r#s/t', 'file:/ex/x/y', 'file:/ex/x/q/r#s/t'],
  ['ftp://ex/x/q/r', 'file:/ex/x/y', 'ftp://ex/x/q/r'],
  ['', 'file:/ex/x/y', 'file:/ex/x/y'],
  ['', 'file:/ex/x/y/', 'file:/ex/x/y/'],
  ['', 'file:/ex/x/y/pdq', 'file:/ex/x/y/pdq'],
  ['z/', 'file:/ex/x/y/', 'file:/ex/x/y/z/'],
  ['file://meetings.example.com/cal#m1',
   'file:/devel/WWW/2000/10/swap/test/reluri-1.n3',
   'file://meetings.example.com/cal#m1'],
  ['file://meetings.example.com/cal#m1',
   'file:/home/connolly/w3ccvs/WWW/2000/10/swap/test/reluri-1.n3',
   'file://meetings.example.com/cal#m1'],
  ['./#blort', 'file:/some/dir/foo', 'file:/some/dir/#blort'],
  ['./#', 'file:/some/dir/foo', 'file:/some/dir/#'],
  // Ryan Lee
  ['./', 'http://example/x/abc.efg', 'http://example/x/'],


  // Graham Klyne's tests
  // http://www.ninebynine.org/Software/HaskellUtils/Network/UriTest.xls
  // 01-31 are from Connelly's cases

  // 32-49
  ['./q:r', 'http://ex/x/y', 'http://ex/x/q:r'],
  ['./p=q:r', 'http://ex/x/y', 'http://ex/x/p=q:r'],
  ['?pp/rr', 'http://ex/x/y?pp/qq', 'http://ex/x/y?pp/rr'],
  ['y/z', 'http://ex/x/y?pp/qq', 'http://ex/x/y/z'],
  ['local/qual@domain.org#frag',
   'mailto:local',
   'mailto:local/qual@domain.org#frag'],
  ['more/qual2@domain2.org#frag',
   'mailto:local/qual1@domain1.org',
   'mailto:local/more/qual2@domain2.org#frag'],
  ['y?q', 'http://ex/x/y?q', 'http://ex/x/y?q'],
  ['/x/y?q', 'http://ex?p', 'http://ex/x/y?q'],
  ['c/d', 'foo:a/b', 'foo:a/c/d'],
  ['/c/d', 'foo:a/b', 'foo:/c/d'],
  ['', 'foo:a/b?c#d', 'foo:a/b?c'],
  ['b/c', 'foo:a', 'foo:b/c'],
  ['../b/c', 'foo:/a/y/z', 'foo:/a/b/c'],
  ['./b/c', 'foo:a', 'foo:b/c'],
  ['/./b/c', 'foo:a', 'foo:/b/c'],
  ['../../d', 'foo://a//b/c', 'foo://a/d'],
  ['.', 'foo:a', 'foo:'],
  ['..', 'foo:a', 'foo:'],

  // 50-57[cf. TimBL comments --
  //  http://lists.w3.org/Archives/Public/uri/2003Feb/0028.html,
  //  http://lists.w3.org/Archives/Public/uri/2003Jan/0008.html)
  ['abc', 'http://example/x/y%2Fz', 'http://example/x/abc'],
  ['../../x%2Fabc', 'http://example/a/x/y/z', 'http://example/a/x%2Fabc'],
  ['../x%2Fabc', 'http://example/a/x/y%2Fz', 'http://example/a/x%2Fabc'],
  ['abc', 'http://example/x%2Fy/z', 'http://example/x%2Fy/abc'],
  ['q%3Ar', 'http://ex/x/y', 'http://ex/x/q%3Ar'],
  ['/x%2Fabc', 'http://example/x/y%2Fz', 'http://example/x%2Fabc'],
  ['/x%2Fabc', 'http://example/x/y/z', 'http://example/x%2Fabc'],
  ['/x%2Fabc', 'http://example/x/y%2Fz', 'http://example/x%2Fabc'],

  // 70-77
  ['local2@domain2', 'mailto:local1@domain1?query1', 'mailto:local2@domain2'],
  ['local2@domain2?query2',
   'mailto:local1@domain1',
   'mailto:local2@domain2?query2'],
  ['local2@domain2?query2',
   'mailto:local1@domain1?query1',
   'mailto:local2@domain2?query2'],
  ['?query2', 'mailto:local@domain?query1', 'mailto:local@domain?query2'],
  ['local@domain?query2', 'mailto:?query1', 'mailto:local@domain?query2'],
  ['?query2', 'mailto:local@domain?query1', 'mailto:local@domain?query2'],
  ['http://example/a/b?c/../d', 'foo:bar', 'http://example/a/b?c/../d'],
  ['http://example/a/b#c/../d', 'foo:bar', 'http://example/a/b#c/../d'],

  // 82-88
  // @isaacs Disagree. Not how browsers do it.
  // ['http:this', 'http://example.org/base/uri', 'http:this'],
  // @isaacs Added
  ['http:this', 'http://example.org/base/uri', 'http://example.org/base/this'],
  ['http:this', 'http:base', 'http:this'],
  ['.//g', 'f:/a', 'f://g'],
  ['b/c//d/e', 'f://example.org/base/a', 'f://example.org/base/b/c//d/e'],
  ['m2@example.ord/c2@example.org',
   'mid:m@example.ord/c@example.org',
   'mid:m@example.ord/m2@example.ord/c2@example.org'],
  ['mini1.xml',
   'file:///C:/DEV/Haskell/lib/HXmlToolbox-3.01/examples/',
   'file:///C:/DEV/Haskell/lib/HXmlToolbox-3.01/examples/mini1.xml'],
  ['../b/c', 'foo:a/y/z', 'foo:a/b/c']
];
relativeTests2.forEach(function(relativeTest) {
  var a = url.resolve(relativeTest[1], relativeTest[0]),
      e = relativeTest[2];
  assert.equal(e, a,
               'resolve(' + [relativeTest[1], relativeTest[0]] + ') == ' + e +
               '\n  actual=' + a);
});

