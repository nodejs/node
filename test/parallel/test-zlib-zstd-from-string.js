'use strict';
// Test compressing and uncompressing a string with zstd

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const inputString = 'ΩΩLorem ipsum dolor sit amet, consectetur adipiscing eli' +
                    't. Morbi faucibus, purus at gravida dictum, libero arcu ' +
                    'convallis lacus, in commodo libero metus eu nisi. Nullam' +
                    ' commodo, neque nec porta placerat, nisi est fermentum a' +
                    'ugue, vitae gravida tellus sapien sit amet tellus. Aenea' +
                    'n non diam orci. Proin quis elit turpis. Suspendisse non' +
                    ' diam ipsum. Suspendisse nec ullamcorper odio. Vestibulu' +
                    'm arcu mi, sodales non suscipit id, ultrices ut massa. S' +
                    'ed ac sem sit amet arcu malesuada fermentum. Nunc sed. ';
const compressedString = 'KLUv/QRYRQkA9tc9H6AlhTb/z/7/gbTI3kaWLKnbCtkZu/hXm0j' +
                         'FpNz/VQM2ADMANQBHTuQOpIYzfVv7XGwXrpoIfgXNAB98xW4wV3' +
                         'vnCF2bjcvWZF2wIZ1vr1mSHHvPHU0TgMGBwUFrF0xqReWcWPO8z' +
                         'Ny6wMwFUilN+Lg987Zvs2GSRMy6uYvtovK9Uuhgst6l9FQrXLnA' +
                         '5gpZL7PdI8bO9sDH3tHm73XBzaUK+LjSPNKRmzQ3ZMYEPozdof1' +
                         '2KcZGfIcLa0PTsdkYqhGcAx/E9mWa8EGEeq0Qou2LTmzgg3YJz/' +
                         '21OuXSF+TOd662d60Qyb04xC5dOF4b8JFH8mpHAxAAELu3tg1oa' +
                         'bBEIWaRHdE0l/+0RdEWWIVMAku8TgbiX/4bU+OpLo4UuY1FKDR8' +
                         'RgBc';

zlib.zstdCompress(inputString, common.mustCall((err, buffer) => {
  assert(inputString.length > buffer.length);

  zlib.zstdDecompress(buffer, common.mustCall((err, buffer) => {
    assert.strictEqual(buffer.toString(), inputString);
  }));
}));

const buffer = Buffer.from(compressedString, 'base64');
zlib.zstdDecompress(buffer, common.mustCall((err, buffer) => {
  assert.strictEqual(buffer.toString(), inputString);
}));
