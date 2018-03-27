'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

// This test ensures all http methods from HTTP parser are exposed
// to http library

const methods = [
  'DELETE',
  'GET',
  'HEAD',
  'POST',
  'PUT',
  'CONNECT',
  'OPTIONS',
  'TRACE',
  'COPY',
  'LOCK',
  'MKCOL',
  'MOVE',
  'PROPFIND',
  'PROPPATCH',
  'SEARCH',
  'UNLOCK',
  'BIND',
  'REBIND',
  'UNBIND',
  'ACL',
  'REPORT',
  'MKACTIVITY',
  'CHECKOUT',
  'MERGE',
  'M-SEARCH',
  'NOTIFY',
  'SUBSCRIBE',
  'UNSUBSCRIBE',
  'PATCH',
  'PURGE',
  'MKCALENDAR',
  'LINK',
  'UNLINK',
  'SOURCE',
];

assert.deepStrictEqual(http.METHODS, methods.sort());
