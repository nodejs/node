'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');


assert.throws(function() {
  http.request(url.parse('file:///whatever'));
}, /Protocol "file:" not supported/);

assert.throws(function() {
  http.request(url.parse('mailto:asdf@asdf.com'));
}, /Protocol "mailto:" not supported/);

assert.throws(function() {
  http.request(url.parse('ftp://www.example.com'));
}, /Protocol "ftp:" not supported/);

assert.throws(function() {
  http.request(url.parse('javascript:alert(\'hello\');'));
}, /Protocol "javascript:" not supported/);

assert.throws(function() {
  http.request(url.parse('xmpp:isaacschlueter@jabber.org'));
}, /Protocol "xmpp:" not supported/);

assert.throws(function() {
  http.request(url.parse('f://some.host/path'));
}, /Protocol "f:" not supported/);
