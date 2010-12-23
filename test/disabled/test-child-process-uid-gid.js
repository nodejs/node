// must be run as sudo, otherwise the gid/uid setting will fail.
var child_process = require("child_process"),
    constants = require("constants"),
    passwd = require("fs").readFileSync("/etc/passwd", "utf8"),
    myUid = process.getuid(),
    myGid = process.getgid();

// get a different user.
// don't care who it is, as long as it's not root
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
if (!otherUid && !otherGid) throw new Error("failed getting passwd info.");

console.error("name, id, gid = %j", [otherName, otherUid, otherGid]);

var whoNumber = child_process.spawn("id",[], {uid:otherUid,gid:otherGid}),
    assert = require("assert");

whoNumber.stdout.buf = "byNumber:";
whoNumber.stdout.on("data", onData);
function onData (c) { this.buf += c; }

whoNumber.on("exit", onExit);
function onExit (code) {
  var buf = this.stdout.buf;
  console.log(buf);
  var expr = new RegExp("^byNumber:uid="+otherUid+"\\("+
                        otherName+"\\) gid="+otherGid+"\\(");
  assert.ok(buf.match(expr), "uid and gid should match "+otherName);
}
