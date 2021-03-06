'use strict';
// Test compressing and uncompressing a string with brotli

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
const compressedString = 'G/gBQBwHdky2aHV5KK9Snf05//1pPdmNw/7232fnIm1IB' +
                         'K1AA8RsN8OB8Nb7Lpgk3UWWUlzQXZyHQeBBbXMTQXC1j7' +
                         'wg3LJs9LqOGHRH2bj/a2iCTLLx8hBOyTqgoVuD1e+Qqdn' +
                         'f1rkUNyrWq6LtOhWgxP3QUwdhKGdZm3rJWaDDBV7+pDk1' +
                         'MIkrmjp4ma2xVi5MsgJScA3tP1I7mXeby6MELozrwoBQD' +
                         'mVTnEAicZNj4lkGqntJe2qSnGyeMmcFgraK94vCg/4iLu' +
                         'Tw5RhKhnVY++dZ6niUBmRqIutsjf5TzwF5iAg8a9UkjF5' +
                         '2eZ0tB2vo6v8SqVfNMkBmmhxr0NT9LkYF69aEjlYzj7IE' +
                         'KmEUQf1HBogRYhFIt4ymRNEgHAIzOyNEsQM=';

zlib.brotliCompress(inputString, common.mustCall((err, buffer) => {
  assert(inputString.length > buffer.length);

  zlib.brotliDecompress(buffer, common.mustCall((err, buffer) => {
    assert.strictEqual(buffer.toString(), inputString);
  }));
}));

const buffer = Buffer.from(compressedString, 'base64');
zlib.brotliDecompress(buffer, common.mustCall((err, buffer) => {
  assert.strictEqual(buffer.toString(), inputString);
}));
