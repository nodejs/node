'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const util = require('util');

const tests = new Array();

let gid = 1;
let uid = 1;

if (!common.isWindows) {
  gid = process.getgid();
  uid = process.getuid();
}

tests['fs.sync.access'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'node.fs.accessSync("fs.txt");' +
                          'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.chmod'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'node.fs.chmodSync("fs.txt",100);' +
                         'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.chown'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                         `node.fs.chownSync("fs.txt", ${uid}, ${gid});` +
                         'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.close'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.copyfile'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                            'node.fs.copyFileSync("fs.txt","a.txt");' +
                            'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.fchmod'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'const fd = node.fs.openSync("fs.txt", "r+");' +
                          'node.fs.fchmodSync(fd,100);' +
                          'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.fchown'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'const fd = node.fs.openSync("fs.txt", "r+");' +
                          `node.fs.fchownSync(fd, ${uid}, ${gid});` +
                          'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.fdatasync'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                             'const fd = node.fs.openSync("fs.txt", "r+");' +
                             'node.fs.fdatasyncSync(fd);' +
                             'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.fstat'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'node.fs.readFileSync("fs.txt");' +
                         'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.fsync'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'const fd = node.fs.openSync("fs.txt", "r+");' +
                         'node.fs.fsyncSync(fd);' +
                         'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.ftruncate'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                             'const fd = node.fs.openSync("fs.txt", "r+");' +
                             'node.fs.ftruncateSync(fd, 1);' +
                             'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.futimes'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                           'const fd = node.fs.openSync("fs.txt", "r+");' +
                           'node.fs.futimesSync(fd,1,1);' +
                           'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.lchown'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                          `node.fs.lchownSync("fs.txt", ${uid}, ${gid});` +
                          'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.link'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'node.fs.linkSync("fs.txt", "linkx");' +
                        'node.fs.unlinkSync("linkx");' +
                        'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.lstat'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'node.fs.lstatSync("fs.txt");' +
                         'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.mkdir'] = 'node.fs.mkdirSync("fstemp");' +
                         'node.fs.rmdirSync("fstemp")';
tests['fs.sync.mkdtemp'] = 'const fp = node.fs.mkdtempSync("fstest");' +
                           'node.fs.rmdirSync(fp)';
tests['fs.sync.open'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.read'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'node.fs.readFileSync("fs.txt");' +
                        'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.readdir'] = 'node.fs.readdirSync("./")';
tests['fs.sync.realpath'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                            'node.fs.linkSync("fs.txt", "linkx");' +
                            'node.fs.realpathSync.native("linkx");' +
                            'node.fs.unlinkSync("linkx");' +
                            'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.rename'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'node.fs.renameSync("fs.txt","xyz.txt"); ' +
                          'node.fs.unlinkSync("xyz.txt")';
tests['fs.sync.rmdir'] = 'node.fs.mkdirSync("fstemp");' +
                         'node.fs.rmdirSync("fstemp")';
tests['fs.sync.stat'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'node.fs.statSync("fs.txt");' +
                        'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.unlink'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'node.fs.linkSync("fs.txt", "linkx");' +
                          'node.fs.unlinkSync("linkx");' +
                          'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.utimes'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'node.fs.utimesSync("fs.txt",1,1);' +
                          'node.fs.unlinkSync("fs.txt")';
tests['fs.sync.write'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'node.fs.unlinkSync("fs.txt")';

// On windows, we need permissions to test symlink and readlink.
// We'll only try to run these tests if we have enough privileges.
if (common.canCreateSymLink()) {
  tests['fs.sync.symlink'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                             'node.fs.symlinkSync("fs.txt", "linkx");' +
                             'node.fs.unlinkSync("linkx");' +
                             'node.fs.unlinkSync("fs.txt")';
  tests['fs.sync.readlink'] = 'node.fs.writeFileSync("fs.txt", "123", "utf8");' +
                              'node.fs.symlinkSync("fs.txt", "linkx");' +
                              'node.fs.readlinkSync("linkx");' +
                              'node.fs.unlinkSync("linkx");' +
                              'node.fs.unlinkSync("fs.txt")';
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const traceFile = path.join(tmpdir.path, 'node_trace.1.log');

for (const tr in tests) {
  const proc = cp.spawnSync(process.execPath,
                            [ '--trace-events-enabled',
                              '--trace-event-categories', 'node.fs.sync',
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
  const traces = JSON.parse(data.toString()).traceEvents;
  assert(traces.length > 0);

  // C++ fs sync trace events should be generated.
  assert(traces.some((trace) => {
    if (trace.pid !== proc.pid)
      return false;
    if (trace.cat !== 'node,node.fs,node.fs.sync')
      return false;
    if (trace.name !== tr)
      return false;
    return true;
  }));
}
