'use strict';

require('../common');

const assert = require('assert');
const { URLPattern } = require('url');

// Regression test for: URLPattern.exec() does not preserve capture group
// declaration order. The groups object returned for each component must
// enumerate its named capture groups in the order they appear in the
// pattern string.

{
  // Three-group pathname pattern: one, two, three
  const pattern = new URLPattern({ pathname: '/:one/:two/:three' });
  const result = pattern.exec('https://example.com/a/b/c');

  assert.ok(result, 'exec should return a result');

  const keys = Object.keys(result.pathname.groups);
  assert.deepStrictEqual(
    keys,
    ['one', 'two', 'three'],
    'pathname capture groups must be enumerated in declaration order',
  );
  assert.strictEqual(result.pathname.groups.one, 'a');
  assert.strictEqual(result.pathname.groups.two, 'b');
  assert.strictEqual(result.pathname.groups.three, 'c');
}

{
  // Reversed alphabetical order to distinguish declaration order from sort
  // order: z comes before a in declaration, so must appear first.
  const pattern = new URLPattern({ pathname: '/:z/:a/:m' });
  const result = pattern.exec('https://example.com/1/2/3');

  assert.ok(result, 'exec should return a result');

  assert.deepStrictEqual(
    Object.keys(result.pathname.groups),
    ['z', 'a', 'm'],
    'groups must follow declaration order even when names are not alphabetical',
  );
}

{
  // Multiple components can each carry named groups independently; verify
  // that declaration order is preserved per-component.
  const pattern = new URLPattern({
    hostname: ':sub.:root',
    pathname: '/:last/:first',
  });
  const result = pattern.exec('https://api.example.com/smith/john');

  assert.ok(result, 'exec should return a result');

  assert.deepStrictEqual(
    Object.keys(result.hostname.groups),
    ['sub', 'root'],
    'hostname groups must follow declaration order',
  );

  assert.deepStrictEqual(
    Object.keys(result.pathname.groups),
    ['last', 'first'],
    'pathname groups must follow declaration order',
  );
}

{
  // A pattern with no named groups should produce an empty groups object.
  const pattern = new URLPattern({ pathname: '/fixed/path' });
  const result = pattern.exec('https://example.com/fixed/path');

  assert.ok(result, 'exec should return a result');
  assert.deepStrictEqual(
    Object.keys(result.pathname.groups),
    [],
    'a pattern without named groups must yield an empty groups object',
  );
}
