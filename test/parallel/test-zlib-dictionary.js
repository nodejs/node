'use strict';
// test compression/decompression with dictionary

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

var spdyDict = new Buffer([
  'optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-',
  'languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi',
  'f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser',
  '-agent10010120020120220320420520630030130230330430530630740040140240340440',
  '5406407408409410411412413414415416417500501502503504505accept-rangesageeta',
  'glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic',
  'ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran',
  'sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati',
  'oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo',
  'ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe',
  'pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic',
  'ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1',
  '.1statusversionurl\0'
].join(''));

var deflate = zlib.createDeflate({ dictionary: spdyDict });

var input = [
  'HTTP/1.1 200 Ok',
  'Server: node.js',
  'Content-Length: 0',
  ''
].join('\r\n');

var called = 0;

//
// We'll use clean-new inflate stream each time
// and .reset() old dirty deflate one
//
function run(num) {
  var inflate = zlib.createInflate({ dictionary: spdyDict });

  if (num === 2) {
    deflate.reset();
    deflate.removeAllListeners('data');
  }

  // Put data into deflate stream
  deflate.on('data', function(chunk) {
    inflate.write(chunk);
  });

  // Get data from inflate stream
  var output = [];
  inflate.on('data', function(chunk) {
    output.push(chunk);
  });
  inflate.on('end', function() {
    called++;

    assert.equal(output.join(''), input);

    if (num < 2) run(num + 1);
  });

  deflate.write(input);
  deflate.flush(function() {
    inflate.end();
  });
}
run(1);

function rawDictionaryTest() {
  let output = '';
  const deflate = zlib.createDeflateRaw({ dictionary: spdyDict });
  const inflate = zlib.createInflateRaw({ dictionary: spdyDict });

  deflate.on('data', function(chunk) {
    inflate.write(chunk);
  });

  inflate.on('data', function(chunk) {
    output += chunk;
  });

  deflate.on('end', function() {
    inflate.end();
  });

  inflate.on('end', common.mustCall(function() {
    assert.equal(input, output);
  }));

  deflate.write(input);
  deflate.end();
}

function deflateRawResetDictionaryTest() {
  let doneReset = false;
  let output = '';
  const deflate = zlib.createDeflateRaw({ dictionary: spdyDict });
  const inflate = zlib.createInflateRaw({ dictionary: spdyDict });

  deflate.on('data', function(chunk) {
    if (doneReset)
      inflate.write(chunk);
  });

  inflate.on('data', function(chunk) {
    output += chunk;
  });

  deflate.on('end', function() {
    inflate.end();
  });

  inflate.on('end', common.mustCall(function() {
    assert.equal(input, output);
  }));

  deflate.write(input);
  deflate.flush(function() {
    deflate.reset();
    doneReset = true;
    deflate.write(input);
    deflate.end();
  });
}

rawDictionaryTest();
deflateRawResetDictionaryTest();

process.on('exit', function() {
  assert.equal(called, 2);
});
