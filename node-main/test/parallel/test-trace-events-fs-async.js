'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const util = require('util');

const tests = { __proto__: null };

let gid = 1;
let uid = 1;

if (!common.isWindows) {
  gid = process.getgid();
  uid = process.getuid();
}

function wrapper(func, args) {
  return `(${func.toString()})(${JSON.stringify(args)})`;
}

function access() {
  const fs = require('fs');
  fs.writeFileSync('fs0.txt', '123', 'utf8');
  fs.access('fs0.txt', () => {
    fs.unlinkSync('fs0.txt');
  });
}

function chmod() {
  const fs = require('fs');
  fs.writeFileSync('fs1.txt', '123', 'utf8');
  fs.chmod('fs1.txt', 100, () => {
    fs.unlinkSync('fs1.txt');
  });
}

function chown({ uid, gid }) {
  const fs = require('fs');
  fs.writeFileSync('fs2.txt', '123', 'utf8');
  fs.chown('fs2.txt', uid, gid, () => {
    fs.unlinkSync('fs2.txt');
  });
}

function close() {
  const fs = require('fs');
  fs.writeFile('fs3.txt', '123', 'utf8', () => {
    fs.unlinkSync('fs3.txt');
  });
}

function copyfile() {
  const fs = require('fs');
  fs.writeFileSync('fs4.txt', '123', 'utf8');
  fs.copyFile('fs4.txt', 'a.txt', () => {
    fs.unlinkSync('fs4.txt');
    fs.unlinkSync('a.txt');
  });
}

function fchmod() {
  const fs = require('fs');
  fs.writeFileSync('fs5.txt', '123', 'utf8');
  const fd = fs.openSync('fs5.txt', 'r+');
  fs.fchmod(fd, 100, () => {
    fs.unlinkSync('fs5.txt');
  });
}

function fchown({ uid, gid }) {
  const fs = require('fs');
  fs.writeFileSync('fs6.txt', '123', 'utf8');
  const fd = fs.openSync('fs6.txt', 'r+');
  fs.fchown(fd, uid, gid, () => {
    fs.unlinkSync('fs6.txt');
  });
}

function fdatasync() {
  const fs = require('fs');
  fs.writeFileSync('fs7.txt', '123', 'utf8');
  const fd = fs.openSync('fs7.txt', 'r+');
  fs.fdatasync(fd, () => {
    fs.unlinkSync('fs7.txt');
  });
}

function fstat() {
  const fs = require('fs');
  fs.writeFileSync('fs8.txt', '123', 'utf8');
  fs.readFile('fs8.txt', () => {
    fs.unlinkSync('fs8.txt');
  });
}

function fsync() {
  const fs = require('fs');
  fs.writeFileSync('fs9.txt', '123', 'utf8');
  const fd = fs.openSync('fs9.txt', 'r+');
  fs.fsync(fd, () => {
    fs.unlinkSync('fs9.txt');
  });
}

function ftruncate() {
  const fs = require('fs');
  fs.writeFileSync('fs10.txt', '123', 'utf8');
  const fd = fs.openSync('fs10.txt', 'r+');
  fs.ftruncate(fd, 1, () => {
    fs.unlinkSync('fs10.txt');
  });
}

function futime() {
  const fs = require('fs');
  fs.writeFileSync('fs11.txt', '123', 'utf8');
  const fd = fs.openSync('fs11.txt', 'r+');
  fs.futimes(fd, 1, 1, () => {
    fs.unlinkSync('fs11.txt');
  });
}

function lutime() {
  const fs = require('fs');
  fs.writeFileSync('fs111.txt', '123', 'utf8');
  fs.lutimes('fs111.txt', 1, 1, () => {
    fs.unlinkSync('fs111.txt');
  });
}

function lchown({ uid, gid }) {
  const fs = require('fs');
  fs.writeFileSync('fs12.txt', '123', 'utf8');
  fs.lchown('fs12.txt', uid, gid, () => {
    fs.unlinkSync('fs12.txt');
  });
}

function link() {
  const fs = require('fs');
  fs.writeFileSync('fs13.txt', '123', 'utf8');
  fs.link('fs13.txt', 'fs14.txt', () => {
    fs.unlinkSync('fs13.txt');
    fs.unlinkSync('fs14.txt');
  });
}

function lstat() {
  const fs = require('fs');
  fs.writeFileSync('fs15.txt', '123', 'utf8');
  fs.lstat('fs15.txt', () => {
    fs.unlinkSync('fs15.txt');
  });
}

function mkdir() {
  const fs = require('fs');
  fs.mkdir('fstemp0', () => {
    fs.rmdir('fstemp0', () => {});
  });
}

function mktmp() {
  fs.mkdtemp('fstemp1', (err, fp) => {
    fs.rmdir(fp, () => {});
  });
}

function open() {
  const fs = require('fs');
  fs.writeFile('fs16.txt', '123', 'utf8', () => {
    fs.unlinkSync('fs16.txt');
  });
}

function read() {
  const fs = require('fs');
  fs.writeFileSync('fs17.txt', '123', 'utf8');
  fs.readFile('fs17.txt', () => {
    fs.unlinkSync('fs17.txt');
  });
}

function readdir() {
  const fs = require('fs');
  fs.readdir('./', () => {});
}

function opendir() {
  const fs = require('fs');
  fs.opendir('./', () => {});
}

function realpath() {
  const fs = require('fs');
  fs.writeFileSync('fs18.txt', '123', 'utf8');
  fs.linkSync('fs18.txt', 'fs19.txt');
  fs.realpath.native('fs19.txt', () => {
    fs.unlinkSync('fs18.txt');
    fs.unlinkSync('fs19.txt');
  });
}

function rename() {
  const fs = require('fs');
  fs.writeFileSync('fs20.txt', '123', 'utf8');
  fs.rename('fs20.txt', 'fs21.txt', () => {
    fs.unlinkSync('fs21.txt');
  });
}

function rmdir() {
  const fs = require('fs');
  fs.mkdirSync('fstemp2');
  fs.rmdir('fstemp2', () => {});
}

function stat() {
  const fs = require('fs');
  fs.writeFileSync('fs22.txt', '123', 'utf8');
  fs.stat('fs22.txt', () => {
    fs.unlinkSync('fs22.txt');
  });
}

function unlink() {
  const fs = require('fs');
  fs.writeFileSync('fs23.txt', '123', 'utf8');
  fs.linkSync('fs23.txt', 'fs24.txt');
  fs.unlink('fs23.txt', () => {});
  fs.unlink('fs24.txt', () => {});
}

function utime() {
  const fs = require('fs');
  fs.writeFileSync('fs25.txt', '123', 'utf8');
  fs.utimes('fs25.txt', 1, 1, () => {
    fs.unlinkSync('fs25.txt');
  });
}

function write() {
  const fs = require('fs');
  fs.writeFile('fs26.txt', '123', 'utf8', () => {
    fs.unlinkSync('fs26.txt');
  });
}

function symlink() {
  const fs = require('fs');
  fs.writeFileSync('fs27.txt', '123', 'utf8');
  fs.symlink('fs27.txt', 'fs28.txt', () => {
    fs.unlinkSync('fs27.txt');
    fs.unlinkSync('fs28.txt');
  });
}

function readlink() {
  const fs = require('fs');
  fs.writeFileSync('fs29.txt', '123', 'utf8');
  fs.symlinkSync('fs29.txt', 'fs30.txt');
  fs.readlink('fs30.txt', () => {
    fs.unlinkSync('fs29.txt');
    fs.unlinkSync('fs30.txt');
  });
}
// The key defined in get_fs_name_by_type function in node_file.cc and node_dir.cc
tests.access = wrapper(access);
tests.chmod = wrapper(chmod);
tests.chown = wrapper(chown, { uid, gid });
tests.close = wrapper(close);
tests.copyfile = wrapper(copyfile);
tests.fchmod = wrapper(fchmod);
tests.fchown = wrapper(fchown, { uid, gid });
tests.fdatasync = wrapper(fdatasync);
tests.fstat = wrapper(fstat);
tests.fsync = wrapper(fsync);
tests.ftruncate = wrapper(ftruncate);
tests.futime = wrapper(futime);
tests.lutime = wrapper(lutime);
tests.lchown = wrapper(lchown, { uid, gid });
tests.link = wrapper(link);
tests.lstat = wrapper(lstat);
tests.mkdir = wrapper(mkdir);
tests.mkdtemp = wrapper(mktmp);
tests.open = wrapper(open);
tests.read = wrapper(read);
tests.scandir = wrapper(readdir);
tests.opendir = wrapper(opendir);
tests.realpath = wrapper(realpath);
tests.rename = wrapper(rename);
tests.rmdir = wrapper(rmdir);
tests.stat = wrapper(stat);
tests.unlink = wrapper(unlink);
tests.utime = wrapper(utime);
tests.write = wrapper(write);

// On windows, we need permissions to test symlink and readlink.
// We'll only try to run these tests if we have enough privileges.
if (common.canCreateSymLink()) {
  tests.symlink = wrapper(symlink);
  tests.readlink = wrapper(readlink);
}
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const traceFile = tmpdir.resolve('node_trace.1.log');

for (const tr in tests) {
  const proc = cp.spawnSync(process.execPath,
                            [ '--trace-events-enabled',
                              '--trace-event-categories', 'node.fs_dir.async,node.fs.async',
                              '-e', tests[tr] ],
                            { cwd: tmpdir.path, encoding: 'utf8' });

  // Make sure the operation is successful.
  // Don't use assert with a custom message here. Otherwise the
  // inspection in the message is done eagerly and wastes a lot of CPU
  // time.
  if (proc.status !== 0) {
    throw new Error(`${tr}:\n${util.inspect(proc)}`);
  }

  // Confirm that trace log file is created.
  assert(fs.existsSync(traceFile));
  const data = fs.readFileSync(traceFile);
  const { traceEvents } = JSON.parse(data.toString());
  // Confirm that the data we want is produced
  const traces = traceEvents.filter((item) => {
    return [
      'node,node.fs,node.fs.async',
      'node,node.fs_dir,node.fs_dir.async',
    ].includes(item.cat);
  });
  // Confirm that the data we want is produced
  assert(traces.length > 0);
  assert(traces.some((trace) => {
    return trace.name === tr;
  }));
}
