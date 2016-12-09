'use strict';
// test compression/decompression with dictionary

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const spdyDict = Buffer.from([
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

const input = [
  'HTTP/1.1 200 Ok',
  'Server: node.js',
  'Content-Length: 0',
  ''
].join('\r\n');

function basicDictionaryTest() {
  let output = '';
  const deflate = zlib.createDeflate({ dictionary: spdyDict });
  const inflate = zlib.createInflate({ dictionary: spdyDict });
  inflate.setEncoding('utf-8');

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
    assert.strictEqual(input, output);
  }));

  deflate.write(input);
  deflate.end();
}

function deflateResetDictionaryTest() {
  let doneReset = false;
  let output = '';
  const deflate = zlib.createDeflate({ dictionary: spdyDict });
  const inflate = zlib.createInflate({ dictionary: spdyDict });
  inflate.setEncoding('utf-8');

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
    assert.strictEqual(input, output);
  }));

  deflate.write(input);
  deflate.flush(function() {
    deflate.reset();
    doneReset = true;
    deflate.write(input);
    deflate.end();
  });
}

function rawDictionaryTest() {
  let output = '';
  const deflate = zlib.createDeflateRaw({ dictionary: spdyDict });
  const inflate = zlib.createInflateRaw({ dictionary: spdyDict });
  inflate.setEncoding('utf-8');

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
    assert.strictEqual(input, output);
  }));

  deflate.write(input);
  deflate.end();
}

function deflateRawResetDictionaryTest() {
  let doneReset = false;
  let output = '';
  const deflate = zlib.createDeflateRaw({ dictionary: spdyDict });
  const inflate = zlib.createInflateRaw({ dictionary: spdyDict });
  inflate.setEncoding('utf-8');

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
    assert.strictEqual(input, output);
  }));

  deflate.write(input);
  deflate.flush(function() {
    deflate.reset();
    doneReset = true;
    deflate.write(input);
    deflate.end();
  });
}

basicDictionaryTest();
deflateResetDictionaryTest();
rawDictionaryTest();
deflateRawResetDictionaryTest();
