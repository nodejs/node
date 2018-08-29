'use strict';
require('../common');
const assert = require('assert');
const url = require('url');

{
  const fileURL = url.pathToFileURL('test/').href;
  assert.ok(fileURL.startsWith('file:///'));
  assert.ok(fileURL.endsWith('/'));
}

{
  const fileURL = url.pathToFileURL('test\\').href;
  assert.ok(fileURL.startsWith('file:///'));
  assert.ok(fileURL.endsWith('/'));
}

{
  const fileURL = url.pathToFileURL('test/%').href;
  assert.ok(fileURL.includes('%25'));
}
