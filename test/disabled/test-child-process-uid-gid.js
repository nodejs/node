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

var assert = require("assert");
var spawn = require("child_process").spawn;
var fs = require('fs');

var myUid = process.getuid();
var myGid = process.getgid();

if (myUid != 0) {
  console.error('must be run as root, otherwise the gid/uid setting will fail.');
  process.exit(1);
}

// get a different user.
// don't care who it is, as long as it's not root
var passwd = fs.readFileSync('/etc/passwd', 'utf8');
passwd = passwd.trim().split(/\n/);

for (var i = 0, l = passwd.length; i < l; i ++) {
  if (passwd[i].charAt(0) === "#") continue;
  passwd[i] = passwd[i].split(":");
  var otherName = passwd[i][0];
  var otherUid = +passwd[i][2];
  var otherGid = +passwd[i][3];
  if (otherUid && otherUid !== myUid &&
      otherGid && otherGid !== myGid &&
      otherUid > 0) {
    break;
  }
}
if (!otherUid && !otherGid) throw new Error('failed getting passwd info.');

console.error('name, id, gid = %j', [otherName, otherUid, otherGid]);

var whoNumber = spawn('id', [], { uid: otherUid, gid: otherGid });
var whoName = spawn('id', [], { uid: otherName, gid: otherGid });

whoNumber.stdout.buf = 'byNumber:';
whoName.stdout.buf = 'byName:';
whoNumber.stdout.on('data', onData);
whoName.stdout.on('data', onData);
function onData (c) { this.buf += c; }

whoNumber.on("exit", onExit);
whoName.on("exit", onExit);

function onExit (code) {
  var buf = this.stdout.buf;
  console.log(buf);
  var expr = new RegExp("^(byName|byNumber):uid=" +
                        otherUid +
                        "\\(" +
                        otherName +
                        "\\) gid=" +
                        otherGid +
                        "\\(");
  assert.ok(buf.match(expr), "uid and gid should match " + otherName);
}
