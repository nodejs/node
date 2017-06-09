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

common.refreshTmpDir();

const filename = join(common.tmpDir, 'test.txt');

const n = 220;
const s = '南越国是前203年至前111年存在于岭南地区的一个国家，国都位于番禺，疆域包括今天中国的广东、' +
          '广西两省区的大部份地区，福建省、湖南、贵州、云南的一小部份地区和越南的北部。' +
          '南越国是秦朝灭亡后，由南海郡尉赵佗于前203年起兵兼并桂林郡和象郡后建立。' +
          '前196年和前179年，南越国曾先后两次名义上臣属于西汉，成为西汉的“外臣”。前112年，' +
          '南越国末代君主赵建德与西汉发生战争，被汉武帝于前111年所灭。南越国共存在93年，' +
          '历经五代君主。南越国是岭南地区的第一个有记载的政权国家，采用封建制和郡县制并存的制度，' +
          '它的建立保证了秦末乱世岭南地区社会秩序的稳定，有效的改善了岭南地区落后的政治、##济现状。\n';

fs.writeFile(filename, s, common.mustCall(function(e) {
  assert.ifError(e);

  fs.readFile(filename, common.mustCall(function(e, buffer) {
    assert.ifError(e);
    assert.strictEqual(Buffer.byteLength(s), buffer.length);
  }));
}));

// test that writeFile accepts buffers
const filename2 = join(common.tmpDir, 'test2.txt');
const buf = Buffer.from(s, 'utf8');

fs.writeFile(filename2, buf, common.mustCall(function(e) {
  assert.ifError(e);

  fs.readFile(filename2, common.mustCall(function(e, buffer) {
    assert.ifError(e);

    assert.strictEqual(buf.length, buffer.length);
  }));
}));

// test that writeFile accepts numbers.
const filename3 = join(common.tmpDir, 'test3.txt');

const m = 0o600;
fs.writeFile(filename3, n, { mode: m }, common.mustCall(function(e) {
  assert.ifError(e);

  // windows permissions aren't unix
  if (!common.isWindows) {
    const st = fs.statSync(filename3);
    assert.strictEqual(st.mode & 0o700, m);
  }

  fs.readFile(filename3, common.mustCall(function(e, buffer) {
    assert.ifError(e);

    assert.strictEqual(Buffer.byteLength('' + n), buffer.length);
  }));
}));

// test that writeFile accepts file descriptors
const filename4 = join(common.tmpDir, 'test4.txt');

fs.open(filename4, 'w+', common.mustCall(function(e, fd) {
  assert.ifError(e);

  fs.writeFile(fd, s, common.mustCall(function(e) {
    assert.ifError(e);

    fs.close(fd, common.mustCall(function(e) {
      assert.ifError(e);

      fs.readFile(filename4, common.mustCall(function(e, buffer) {
        assert.ifError(e);

        assert.strictEqual(Buffer.byteLength(s), buffer.length);
      }));
    }));
  }));
}));

process.on('exit', function() {
  fs.unlinkSync(filename);
  fs.unlinkSync(filename2);
  fs.unlinkSync(filename3);
  fs.unlinkSync(filename4);
});
