import '../common/index.mjs';

import assert from 'assert';

import { referenceToLocalMdFile } from '../../tools/doc/markdown.mjs';

{
  const shouldBeSpotted = [
    'test.md',
    'TEST.MD',
    'test.js.md',
    '.test.md',
    './test.md',
    'subfolder/test.md',
    '../test.md',
    'test.md#anchor',
    'subfolder/test.md#anchor',
    '/test.md',
  ];

  shouldBeSpotted.forEach((url) => {
    assert.match(url, referenceToLocalMdFile);
  });
}

{
  const shouldNotBeSpotted = [
    'https://example.com/test.md',
    'HTTPS://EXAMPLE.COM/TEST.MD',
    'git+https://example.com/test.md',
    'ftp://1.1.1.1/test.md',
    'urn:isbn:9780307476463.md',
    'file://./test.md',
    '/dev/null',
    'test.html',
    'test.html#anchor.md',
    'test.html?anchor.md',
    'test.md5',
    'testmd',
    '.md',
  ];

  shouldNotBeSpotted.forEach((url) => {
    assert.doesNotMatch(url, referenceToLocalMdFile);
  });
}
