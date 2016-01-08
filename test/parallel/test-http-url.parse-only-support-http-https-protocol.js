'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

const tests = [
  [
   'file:///whatever',
   /^Protocol 'file:' not supported. Expected 'http:'/
  ],
  [
    'mailto:asdf@asdf.com',
    /^Protocol 'mailto:' not supported. Expected 'http:'/
  ],
  [
    'ftp://www.example.com',
    /^Protocol 'ftp:' not supported. Expected 'http:'/
  ],
  [
    'javascript:alert(\'hello\');',
    /^Protocol 'javascript:' not supported. Expected 'http:'/
  ],
  [
    'xmpp:isaacschlueter@jabber.org',
    /^Protocol 'xmpp:' not supported. Expected 'http:'/
  ],
  [
    'f://some.host/path',
    /^Protocol 'f:' not supported. Expected 'http:'/
  ]
];

for (var test of tests) {
  assert.throws(
    () => {
      http.request(url.parse(test[0]));
    },
    (err) => {
      if (err instanceof Error) {
        assert(test[1].test(err.message));
        return true;
      }
    }
  );
}
