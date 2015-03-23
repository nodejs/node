'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');
const url = require('url');

// url: The url object to test (if string, it's parsed to url)
// set: Object where the fields are the property values to set in the url object
// test: Object where the fields are the property values to test in the url object
// Example:
//
// Tests that the host property reflects the new port that was set on the url object
// {
//   url: 'http://www.google.com',
//   set: {port: 443},
//   test: {host: 'www.google.com:443'}
// }

const accessorTests = [{
  // Setting href to null nullifies all properties.
  url: 'http://user:password@www.google.com:80/path?query#hash',
  set: {href: null},
  test: {
    protocol: null,
    port: null,
    query: null,
    auth: null,
    hostname: null,
    host: null,
    pathname: null,
    hash: null,
    search: null,
    href: ''
  }
}, {
  // Resetting href doesn't forget query string parsing preference
  url: url.parse('http://www.google.com?key=value', true),
  set: {href: 'http://www.yahoo.com?key2=value2'},
  test: {
    query: {key2: 'value2'}
  }
}, {
  // Setting href to non-null non-string coerces to string
  url: 'google',
  set: {href: undefined},
  test: {
    path: 'undefined',
    href: 'undefined'
  }
}, {
  // Setting port is reflected in host
  url: 'http://www.google.com',
  set: {port: 443},
  test: {
    host: 'www.google.com:443',
    hostname: 'www.google.com',
    port: '443'
  }
}, {
  // Port is treated as an unsigned 16-bit integer
  url: 'http://www.google.com',
  set: {port: 100000},
  test: {
    host: 'www.google.com:34464',
    hostname: 'www.google.com',
    port: '34464'
  }
}, {
  // Port is parsed numerically
  url: 'http://www.google.com',
  set: {port: '100000'},
  test: {
    host: 'www.google.com:34464',
    hostname: 'www.google.com',
    port: '34464'
  }
}, {
  // Invalid port values become 0
  url: 'http://www.google.com',
  set: {port: NaN},
  test: {
    host: 'www.google.com:0',
    hostname: 'www.google.com',
    port: '0'
  }
}, {
  // Auth special characters are urlencoded
  url: 'http://www.google.com',
  set: {auth: 'weird@'},
  test: {
    auth: 'weird@',
    href: 'http://weird%40@www.google.com/'
  }
}, {
  // Auth password separator is not urlencoded
  url: 'http://www.google.com',
  set: {auth: 'weird@:password:'},
  test: {
    auth: 'weird@:password:',
    href: 'http://weird%40:password%3A@www.google.com/'
  }
}, {

  // Host is reflected in hostname and port
  url: 'http://www.google.com',
  set: {host: 'www.yahoo.com:443'},
  test: {
    hostname: 'www.yahoo.com',
    host: 'www.yahoo.com:443',
    port: '443',
  }
}, {

  // Port in host has port setter behavior
  url: 'http://www.google.com',
  set: {host: 'www.yahoo.com:100000'},
  test: {
    hostname: 'www.yahoo.com',
    host: 'www.yahoo.com:34464',
    port: '34464',
  }
}, {

  // Hostname in host has hostname setter behavior
  url: 'http://www.google.com',
  set: {host: 'www.yahoo .com'},
  test: {
    hostname: 'www.yahoo%20.com',
    host: 'www.yahoo%20.com',
    port: null,
    href: 'http://www.yahoo%20.com/'
  }
}, {

  // Nullifying host nullifies port and hostname
  url: 'http://www.google.com:443',
  set: {host: null},
  test: {
    hostname: null,
    host: null,
    port: null
  }
}, {

  // Question mark is added to search
  url: 'http://www.google.com',
  set: {search: 'key=value'},
  test: {
    search: '?key=value'
  }
}, {

  // It's impossible to start a hash in search
  url: 'http://www.google.com',
  set: {search: 'key=value#hash'},
  test: {
    search: '?key=value%23hash'
  }
}, {

  // Query is reflected in urls that parse query strings
  url: url.parse('http://www.google.com', true),
  set: {search: 'key=value'},
  test: {
    search: '?key=value',
    query: {key: 'value'}
  }
}, {

  // Search is reflected in path
  url: 'http://www.google.com',
  set: {search: 'key=value'},
  test: {
    path: '/?key=value'
  }
}, {

  // Empty search is ok
  url: 'http://www.google.com',
  set: {search: ''},
  test: {
    search: '?',
    path: '/?'
  }
}, {

  // Nullifying search is ok
  url: 'http://www.google.com?key=value',
  set: {search: null},
  test: {
    search: null,
    path: '/'
  }
}, {

  // # is added to hash if it's missing
  url: 'http://www.google.com',
  set: {hash: 'myhash'},
  test: {
    hash: '#myhash'
  }
}, {

  // Empty hash is ok
  url: 'http://www.google.com',
  set: {hash: ''},
  test: {
    hash: '#'
  }
}, {

  // Nullifying hash is ok
  url: 'http://www.google.com#hash',
  set: {hash: null},
  test: {
    hash: null
  }
}, {

  // / is added to pathname
  url: 'http://www.google.com',
  set: {pathname: 'path'},
  test: {
    pathname: '/path'
  }
}, {

  // It's impossible to start query in pathname
  url: 'http://www.google.com',
  set: {pathname: 'path?key=value'},
  test: {
    pathname: '/path%3Fkey=value'
  }
}, {

  // It's impossible to start hash in pathname
  url: 'http://www.google.com',
  set: {pathname: 'path#hash'},
  test: {
    pathname: '/path%23hash'
  }
}, {
  // Backslashes are treated as slashes in pathnames
  url: 'http://www.google.com',
  set: {pathname: '\\path\\name'},
  test: {
    pathname: '/path/name'
  }
}, {
  // Empty path is ok
  url: 'http://www.google.com',
  set: {pathname: ''},
  test: {
    pathname: '/'
  }
}, {
  // Null path is ok
  url: 'http://www.google.com/path',
  set: {pathname: null},
  test: {
    pathname: null
  }
}, {
  // Pathname is reflected in path
  url: 'http://www.google.com/path',
  set: {pathname: '/path2'},
  test: {
    path: '/path2'
  }
}, {
  // Protocol doesn't require colon
  url: 'http://www.google.com/path',
  set: {protocol: 'mailto'},
  test: {
    protocol: 'mailto:'
  }
}, {
  // Invalid protocol doesn't change protocol
  url: 'http://www.google.com/path',
  set: {protocol: 'mailto '},
  test: {
    protocol: 'http:'
  }
}, {
  // Nullifying protocol is ok
  url: 'http://www.google.com/path',
  set: {protocol: null},
  test: {
    protocol: null
  }
}, {
  // Empty protocol is invalid
  url: 'http://www.google.com/path',
  set: {protocol: ''},
  test: {
    protocol: 'http:'
  }
}, {
  // Set query to an object
  url: 'http://www.google.com/path',
  set: {query: {key: 'value'}},
  test: {
    search: '?key=value',
    href: 'http://www.google.com/path?key=value'
  }
}, {
  // Set query to null is ok
  url: 'http://www.google.com/path?key=value',
  set: {query: null},
  test: {
    search: null,
    query: null,
    href: 'http://www.google.com/path'
  }
}, {
  // path is reflected in search and pathname
  url: 'http://www.google.com/path?key=value',
  set: {path: 'path2?key2=value2'},
  test: {
    pathname: '/path2',
    search: '?key2=value2',
    href: 'http://www.google.com/path2?key2=value2'
  }
}, {
  // path is reflected in search and pathname 2
  url: 'http://www.google.com/path?key=value',
  set: {path: '?key2=value2'},
  test: {
    pathname: '/',
    search: '?key2=value2',
    href: 'http://www.google.com/?key2=value2'
  }
}, {
  // path is reflected in search and pathname 3
  url: 'http://www.google.com/path?key=value',
  set: {path: 'path2'},
  test: {
    pathname: '/path2',
    search: null,
    href: 'http://www.google.com/path2'
  }
}, {
  // path is reflected in search and pathname 4
  url: 'http://www.google.com/path?key=value',
  set: {path: null},
  test: {
    pathname: null,
    search: null,
    href: 'http://www.google.com'
  }
}

];


accessorTests.forEach(function(test) {
  var urlObject = test.url;

  if (typeof urlObject === 'string')
    urlObject = url.parse(urlObject);

  Object.keys(test.set).forEach(function(key) {
    urlObject[key] = test.set[key];
  });

  Object.keys(test.test).forEach(function(key) {
    const actual = urlObject[key];
    const expected = test.test[key];

    const message = `Expecting ${key}=${util.inspect(expected)} ` +
                  `but got ${key}=${util.inspect(actual)}. ` +
                  `URL object: ${JSON.stringify(urlObject, null, 2)} ` +
                  `\nTest object: ${JSON.stringify(test, null, 2)}`

    assert.deepStrictEqual(actual, expected, message);
  });
});
