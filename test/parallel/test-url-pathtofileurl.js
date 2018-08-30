'use strict';
const { isWindows } = require('../common');
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
  if (isWindows)
    assert.ok(fileURL.endsWith('/'));
  else
    assert.ok(fileURL.endsWith('%5C'));
}

{
  const fileURL = url.pathToFileURL('test/%').href;
  assert.ok(fileURL.includes('%25'));
}
