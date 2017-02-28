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

'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const join = require('path').join;

const filename = join(common.tmpDir, 'append.txt');

const currentFileData = 'ABCD';

const n = 220;
const s = '南越国是前203年至前111年存在于岭南地区的一个国家，国都位于番禺，疆域包括今天中国的广东、' +
          '广西两省区的大部份地区，福建省、湖南、贵州、云南的一小部份地区和越南的北部。' +
          '南越国是秦朝灭亡后，由南海郡尉赵佗于前203年起兵兼并桂林郡和象郡后建立。' +
          '前196年和前179年，南越国曾先后两次名义上臣属于西汉，成为西汉的“外臣”。前112年，' +
          '南越国末代君主赵建德与西汉发生战争，被汉武帝于前111年所灭。南越国共存在93年，' +
          '历经五代君主。南越国是岭南地区的第一个有记载的政权国家，采用封建制和郡县制并存的制度，' +
          '它的建立保证了秦末乱世岭南地区社会秩序的稳定，有效的改善了岭南地区落后的政治、##济现状。\n';

let ncallbacks = 0;

common.refreshTmpDir();

// test that empty file will be created and have content added
fs.appendFile(filename, s, function(e) {
  assert.ifError(e);

  ncallbacks++;

  fs.readFile(filename, function(e, buffer) {
    assert.ifError(e);
    ncallbacks++;
    assert.strictEqual(Buffer.byteLength(s), buffer.length);
  });
});

// test that appends data to a non empty file
const filename2 = join(common.tmpDir, 'append2.txt');
fs.writeFileSync(filename2, currentFileData);

fs.appendFile(filename2, s, function(e) {
  assert.ifError(e);

  ncallbacks++;

  fs.readFile(filename2, function(e, buffer) {
    assert.ifError(e);
    ncallbacks++;
    assert.strictEqual(Buffer.byteLength(s) + currentFileData.length,
                       buffer.length);
  });
});

// test that appendFile accepts buffers
const filename3 = join(common.tmpDir, 'append3.txt');
fs.writeFileSync(filename3, currentFileData);

const buf = Buffer.from(s, 'utf8');

fs.appendFile(filename3, buf, function(e) {
  assert.ifError(e);

  ncallbacks++;

  fs.readFile(filename3, function(e, buffer) {
    assert.ifError(e);
    ncallbacks++;
    assert.strictEqual(buf.length + currentFileData.length, buffer.length);
  });
});

// test that appendFile accepts numbers.
const filename4 = join(common.tmpDir, 'append4.txt');
fs.writeFileSync(filename4, currentFileData);

const m = 0o600;
fs.appendFile(filename4, n, { mode: m }, function(e) {
  assert.ifError(e);

  ncallbacks++;

  // windows permissions aren't unix
  if (!common.isWindows) {
    const st = fs.statSync(filename4);
    assert.strictEqual(st.mode & 0o700, m);
  }

  fs.readFile(filename4, function(e, buffer) {
    assert.ifError(e);
    ncallbacks++;
    assert.strictEqual(Buffer.byteLength(String(n)) + currentFileData.length,
                       buffer.length);
  });
});

// test that appendFile accepts file descriptors
const filename5 = join(common.tmpDir, 'append5.txt');
fs.writeFileSync(filename5, currentFileData);

fs.open(filename5, 'a+', function(e, fd) {
  assert.ifError(e);

  ncallbacks++;

  fs.appendFile(fd, s, function(e) {
    assert.ifError(e);

    ncallbacks++;

    fs.close(fd, function(e) {
      assert.ifError(e);

      ncallbacks++;

      fs.readFile(filename5, function(e, buffer) {
        assert.ifError(e);

        ncallbacks++;
        assert.strictEqual(Buffer.byteLength(s) + currentFileData.length,
                           buffer.length);
      });
    });
  });
});

// test that a missing callback emits a warning, even if the last argument is a
// function.
const filename6 = join(common.tmpDir, 'append6.txt');
const warn = 'Calling an asynchronous function without callback is deprecated.';
common.expectWarning('DeprecationWarning', warn);
fs.appendFile(filename6, console.log);

process.on('exit', function() {
  assert.strictEqual(12, ncallbacks);

  fs.unlinkSync(filename);
  fs.unlinkSync(filename2);
  fs.unlinkSync(filename3);
  fs.unlinkSync(filename4);
  fs.unlinkSync(filename5);
});
