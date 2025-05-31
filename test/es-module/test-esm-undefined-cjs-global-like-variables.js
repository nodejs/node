'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { pathToFileURL } = require('url');

assert.rejects(
  import('data:text/javascript,require;'),
  /require is not defined in ES module scope/
).then(common.mustCall());
assert.rejects(
  import('data:text/javascript,exports={};'),
  /exports is not defined in ES module scope$/
).then(common.mustCall());

assert.rejects(
  import('data:text/javascript,require_custom;'),
  /^(?!in ES module scope)(?!use import instead).*$/
).then(common.mustCall());

const pkgUrl = pathToFileURL(fixtures.path('/es-modules/package-type-module/'));
assert.rejects(
  import(new URL('./cjs.js', pkgUrl)),
  /use the '\.cjs' file extension/
).then(common.mustCall());
assert.rejects(
  import(new URL('./cjs.js#target', pkgUrl)),
  /use the '\.cjs' file extension/
).then(common.mustCall());
assert.rejects(
  import(new URL('./cjs.js?foo=bar', pkgUrl)),
  /use the '\.cjs' file extension/
).then(common.mustCall());
assert.rejects(
  import(new URL('./cjs.js?foo=bar#target', pkgUrl)),
  /use the '\.cjs' file extension/
).then(common.mustCall());

assert.rejects(
  import('data:text/javascript,require;//.js'),
  /^(?:(?!use the '\.cjs' file extension)[\s\S])*$/
).then(common.mustCall());
