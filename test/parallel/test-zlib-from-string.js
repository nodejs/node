'use strict';
// test compressing and uncompressing a string with zlib

require('../common');
var assert = require('assert');
var zlib = require('zlib');

var inputString = 'ΩΩLorem ipsum dolor sit amet, consectetur adipiscing elit.' +
                  ' Morbi faucibus, purus at gravida dictum, libero arcu conv' +
                  'allis lacus, in commodo libero metus eu nisi. Nullam commo' +
                  'do, neque nec porta placerat, nisi est fermentum augue, vi' +
                  'tae gravida tellus sapien sit amet tellus. Aenean non diam' +
                  ' orci. Proin quis elit turpis. Suspendisse non diam ipsum.' +
                  ' Suspendisse nec ullamcorper odio. Vestibulum arcu mi, sod' +
                  'ales non suscipit id, ultrices ut massa. Sed ac sem sit am' +
                  'et arcu malesuada fermentum. Nunc sed. ';
var expectedBase64Deflate = 'eJxdUUtOQzEMvMoc4OndgT0gJCT2buJWlpI4jePeqZfpmXAK' +
                            'LRKbLOzx/HK73q6vOrhCunlF1qIDJhNUeW5I2ozT5OkDlKWL' +
                            'JWkncJG5403HQXAkT3Jw29B9uIEmToMukglZ0vS6ociBh4JG' +
                            '8sV4oVLEUCitK2kxq1WzPnChHDzsaGKy491LofoAbWh8do43' +
                            'oeuYhB5EPCjcLjzYJo48KrfQBvnJecNFJvHT1+RSQsGoC7dn' +
                            '2t/xjhduTA1NWyQIZR0pbHwMDatnD+crPqKSqGPHp1vnlsWM' +
                            '/07ubf7bheF7kqSj84Bm0R1fYTfaK8vqqqfKBtNMhe3OZh6N' +
                            '95CTvMX5HJJi4xOVzCgUOIMSLH7wmeOHaFE4RdpnGavKtrB5' +
                            'xzfO/Ll9';
var expectedBase64Gzip = 'H4sIAAAAAAAAA11RS05DMQy8yhzg6d2BPSAkJPZu4laWkjiN496' +
                         'pl+mZcAotEpss7PH8crverq86uEK6eUXWogMmE1R5bkjajNPk6Q' +
                         'OUpYslaSdwkbnjTcdBcCRPcnDb0H24gSZOgy6SCVnS9LqhyIGHg' +
                         'kbyxXihUsRQKK0raTGrVbM+cKEcPOxoYrLj3Uuh+gBtaHx2jjeh' +
                         '65iEHkQ8KNwuPNgmjjwqt9AG+cl5w0Um8dPX5FJCwagLt2fa3/G' +
                         'OF25MDU1bJAhlHSlsfAwNq2cP5ys+opKoY8enW+eWxYz/Tu5t/t' +
                         'uF4XuSpKPzgGbRHV9hN9ory+qqp8oG00yF7c5mHo33kJO8xfkck' +
                         'mLjE5XMKBQ4gxIsfvCZ44doUThF2mcZq8q2sHnHNzRtagj5AQAA';

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

var buffer = Buffer.from(expectedBase64Deflate, 'base64');
zlib.unzip(buffer, function(err, buffer) {
  assert.equal(buffer.toString(), inputString,
               'decoded inflated string should match');
});

buffer = Buffer.from(expectedBase64Gzip, 'base64');
zlib.unzip(buffer, function(err, buffer) {
  assert.equal(buffer.toString(), inputString,
               'decoded gunzipped string should match');
});
