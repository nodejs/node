'use strict';

require('../common');

const assert = require('assert');
const { URL } = require('url');

function t(expectedUrl, actualConfig) {
  const url = URL.from(actualConfig);
  assert.strictEqual(String(url), expectedUrl);
}

t(':', {});
t(':', []);
t(':', undefined);
t(':', null);
t(':', false);
t(':', 0);
t(':', 42);
t(':', new Date());
t(':', class {});
t(':', 'str');
t(':', new RegExp());
t(':', Symbol());
t('https://nodejs.org', new Proxy({}, {
  get(target, p) {
    if (p === 'protocol')
      return 'https';
    else if (p === 'host')
      return 'nodejs.org';
  }
}));

t('https://nodejs.org', {
  protocol: 'https',
  host: 'nodejs.org'
});

t('https://root@site.com', {
  protocol: 'https',
  host: 'site.com',
  username: 'root'
});

t('https://:1234@site.com', {
  protocol: 'https',
  host: 'site.com',
  password: '1234'
});

t('https://root:1234@site.com', {
  protocol: 'https',
  host: 'site.com',
  username: 'root',
  password: '1234'
});

t('https://site.com?a=1&b=2', {
  protocol: 'https',
  host: 'site.com',
  query: 'a=1&b=2'
});

t('https://site.com/one/two', {
  protocol: 'https',
  host: 'site.com',
  path: ['one', 'two']
});

t('http://localhost:3000', {
  protocol: 'http',
  host: 'localhost',
  port: '3000',
});

t('http://localhost#fr', {
  protocol: 'http',
  host: 'localhost',
  fragment: 'fr'
});

t('http://root:1234@localhost:3000/main?el=%3Cdiv%20%2F%3E#app', {
  protocol: 'http',
  username: 'root',
  password: '1234',
  port: '3000',
  host: 'localhost',
  path: ['main'],
  fragment: 'app',
  query: 'el=%3Cdiv%20%2F%3E'
});
