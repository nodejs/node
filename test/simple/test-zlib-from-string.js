// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// test compressing and uncompressing a string with zlib

var common = require('../common.js');
var assert = require('assert');
var zlib = require('zlib');

var inputString = 'ΩΩLorem ipsum dolor sit amet, consectetur adipiscing el' +
    'it. Morbi faucibus, purus at gravida dictum, libero arcu convallis la' +
    'cus, in commodo libero metus eu nisi. Nullam commodo, neque nec porta' +
    ' placerat, nisi est fermentum augue, vitae gravida tellus sapien sit ' +
    'amet tellus. Aenean non diam orci. Proin quis elit turpis. Suspendiss' +
    'e non diam ipsum. Suspendisse nec ullamcorper odio. Vestibulum arcu m' +
    'i, sodales non suscipit id, ultrices ut massa. Sed ac sem sit amet ar' +
    'cu malesuada fermentum. Nunc sed. ';
var expectedBase64Deflate = 'eJxdUUtOQzEMvMoc4OndgT0gJCT2buJWlpI4jePeqZfpm' +
    'XAKLRKbLOzx/HK73q6vOrhCunlF1qIDJhNUeW5I2ozT5OkDlKWLJWkncJG5403HQXAkT3' +
    'Jw29B9uIEmToMukglZ0vS6ociBh4JG8sV4oVLEUCitK2kxq1WzPnChHDzsaGKy491Lofo' +
    'AbWh8do43oeuYhB5EPCjcLjzYJo48KrfQBvnJecNFJvHT1+RSQsGoC7dn2t/xjhduTA1N' +
    'WyQIZR0pbHwMDatnD+crPqKSqGPHp1vnlsWM/07ubf7bheF7kqSj84Bm0R1fYTfaK8vqq' +
    'qfKBtNMhe3OZh6N95CTvMX5HJJi4xOVzCgUOIMSLH7wmeOHaFE4RdpnGavKtrB5xzfO/Ll9';
var expectedBase64Gzip = 'H4sIAAAAAAAAA11RS05DMQy8yhzg6d2BPSAkJPZu4laWkjiN' +
    '496pl+mZcAotEpss7PH8crverq86uEK6eUXWogMmE1R5bkjajNPk6QOUpYslaSdwkbnjT' +
    'cdBcCRPcnDb0H24gSZOgy6SCVnS9LqhyIGHgkbyxXihUsRQKK0raTGrVbM+cKEcPOxoYr' +
    'Lj3Uuh+gBtaHx2jjeh65iEHkQ8KNwuPNgmjjwqt9AG+cl5w0Um8dPX5FJCwagLt2fa3/G' +
    'OF25MDU1bJAhlHSlsfAwNq2cP5ys+opKoY8enW+eWxYz/Tu5t/tuF4XuSpKPzgGbRHV9h' +
    'N9ory+qqp8oG00yF7c5mHo33kJO8xfkckmLjE5XMKBQ4gxIsfvCZ44doUThF2mcZq8q2s' +
    'HnHNzRtagj5AQAA';

zlib.deflate(inputString, function(err, buffer) {
  assert.equal(buffer.toString('base64'), expectedBase64Deflate,
               'deflate encoded string should match');
});

zlib.gzip(inputString, function(err, buffer) {
  // Can't actually guarantee that we'll get exactly the same
  // deflated bytes when we compress a string, since the header
  // depends on stuff other than the input string itself.
  // However, decrypting it should definitely yield the same
  // result that we're expecting, and this should match what we get
  // from inflating the known valid deflate data.
  zlib.gunzip(buffer, function(err, gunzipped) {
    assert.equal(gunzipped.toString(), inputString,
                 'Should get original string after gzip/gunzip');
  });
});

var buffer = new Buffer(expectedBase64Deflate, 'base64');
zlib.unzip(buffer, function(err, buffer) {
  assert.equal(buffer.toString(), inputString,
               'decoded inflated string should match');
});

buffer = new Buffer(expectedBase64Gzip, 'base64');
zlib.unzip(buffer, function(err, buffer) {
  assert.equal(buffer.toString(), inputString,
               'decoded gunzipped string should match');
});
