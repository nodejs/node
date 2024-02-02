'use strict';
require('../common');
const { fileURLToPath } = require('url');
const assert = require('assert');

{
  const testCases = [
    'file:///path/to/file1.js',
    'file:///C:/Users/Username/Documents/file2.js',
  ];

  testCases.forEach((url) => {
    let parsedPath;
    try {
      parsedPath = fileURLToPath(url);
      assert.ok(parsedPath !== null, `Failed to parse URL: ${url}`);
    } catch (error) {
      throw new Error(`Failed to parse URL: ${url}. Error: ${error}`);
    }
  });
}
