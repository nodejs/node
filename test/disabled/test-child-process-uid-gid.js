'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var fs = require('fs');

var myUid = process.getuid();
var myGid = process.getgid();

if (myUid != 0) {
  console.error('must be run as root, otherwise the gid/uid setting will' +
                ' fail.');
  process.exit(1);
}

// get a different user.
// don't care who it is, as long as it's not root
var passwd = fs.readFileSync('/etc/passwd', 'utf8');
passwd = passwd.trim().split(/\n/);

for (var i = 0, l = passwd.length; i < l; i++) {
  if (passwd[i].charAt(0) === '#') continue;
  passwd[i] = passwd[i].split(':');
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
function onData(c) { this.buf += c; }

whoNumber.on('exit', onExit);
whoName.on('exit', onExit);

function onExit(code) {
  var buf = this.stdout.buf;
  console.log(buf);
  var expr = new RegExp('^(byName|byNumber):uid=' +
                        otherUid +
                        '\\(' +
                        otherName +
                        '\\) gid=' +
                        otherGid +
                        '\\(');
  assert.ok(buf.match(expr), 'uid and gid should match ' + otherName);
}
