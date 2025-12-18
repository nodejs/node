const fs = require('node:fs')
const path = require('node:path')
const assert = require('node:assert');

{
  fs.readFileSync(__filename);
  console.log('Read its own contents') // Should not throw
}
{
  const simpleLoaderPath = path.join(__dirname, 'simple-loader.js');
  fs.readFile(simpleLoaderPath, (err) => {
    assert.ok(err.code, 'ERR_ACCESS_DENIED');
    assert.ok(err.permission, 'FileSystemRead');
  }); // Should throw ERR_ACCESS_DENIED
}