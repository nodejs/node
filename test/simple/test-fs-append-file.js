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

var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var join = require('path').join;

var filename = join(common.tmpDir, 'append.txt');

common.error('appending to ' + filename);

var currentFileData = 'ABCD';

var n = 220;
var s = '南越国是前203年至前111年存在于岭南地区的一个国家，国都位于番禺，疆域包括今天中国的广东、' +
        '广西两省区的大部份地区，福建省、湖南、贵州、云南的一小部份地区和越南的北部。' +
        '南越国是秦朝灭亡后，由南海郡尉赵佗于前203年起兵兼并桂林郡和象郡后建立。' +
        '前196年和前179年，南越国曾先后两次名义上臣属于西汉，成为西汉的“外臣”。前112年，' +
        '南越国末代君主赵建德与西汉发生战争，被汉武帝于前111年所灭。南越国共存在93年，' +
        '历经五代君主。南越国是岭南地区的第一个有记载的政权国家，采用封建制和郡县制并存的制度，' +
        '它的建立保证了秦末乱世岭南地区社会秩序的稳定，有效的改善了岭南地区落后的政治、##济现状。\n';

var ncallbacks = 0;

// test that empty file will be created and have content added
fs.appendFile(filename, s, function(e) {
  if (e) throw e;

  ncallbacks++;
  common.error('appended to file');

  fs.readFile(filename, function(e, buffer) {
    if (e) throw e;
    common.error('file read');
    ncallbacks++;
    assert.equal(Buffer.byteLength(s), buffer.length);
  });
});

// test that appends data to a non empty file
var filename2 = join(common.tmpDir, 'append2.txt');
fs.writeFileSync(filename2, currentFileData);

fs.appendFile(filename2, s, function(e) {
  if (e) throw e;

  ncallbacks++;
  common.error('appended to file2');

  fs.readFile(filename2, function(e, buffer) {
    if (e) throw e;
    common.error('file2 read');
    ncallbacks++;
    assert.equal(Buffer.byteLength(s) + currentFileData.length, buffer.length);
  });
});

// test that appendFile accepts buffers
var filename3 = join(common.tmpDir, 'append3.txt');
fs.writeFileSync(filename3, currentFileData);

var buf = new Buffer(s, 'utf8');
common.error('appending to ' + filename3);

fs.appendFile(filename3, buf, function(e) {
  if (e) throw e;

  ncallbacks++;
  common.error('appended to file3');

  fs.readFile(filename3, function(e, buffer) {
    if (e) throw e;
    common.error('file3 read');
    ncallbacks++;
    assert.equal(buf.length + currentFileData.length, buffer.length);
  });
});

// test that appendFile accepts numbers.
var filename4 = join(common.tmpDir, 'append4.txt');
fs.writeFileSync(filename4, currentFileData);

common.error('appending to ' + filename4);

var m = 0600;
fs.appendFile(filename4, n, { mode: m }, function(e) {
  if (e) throw e;

  ncallbacks++;
  common.error('appended to file4');

  // windows permissions aren't unix
  if (process.platform !== 'win32') {
    var st = fs.statSync(filename4);
    assert.equal(st.mode & 0700, m);
  }

  fs.readFile(filename4, function(e, buffer) {
    if (e) throw e;
    common.error('file4 read');
    ncallbacks++;
    assert.equal(Buffer.byteLength('' + n) + currentFileData.length,
                 buffer.length);
  });
});

process.on('exit', function() {
  common.error('done');
  assert.equal(8, ncallbacks);

  fs.unlinkSync(filename);
  fs.unlinkSync(filename2);
  fs.unlinkSync(filename3);
  fs.unlinkSync(filename4);
});
