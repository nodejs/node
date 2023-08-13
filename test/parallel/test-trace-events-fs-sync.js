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

tests['fs.sync.access'] = 'fs.writeFileSync("fs0.txt", "123", "utf8");' +
                          'fs.accessSync("fs0.txt");' +
                          'fs.unlinkSync("fs0.txt")';
tests['fs.sync.chmod'] = 'fs.writeFileSync("fs1.txt", "123", "utf8");' +
                         'fs.chmodSync("fs1.txt",100);' +
                         'fs.unlinkSync("fs1.txt")';
tests['fs.sync.chown'] = 'fs.writeFileSync("fs2.txt", "123", "utf8");' +
                         `fs.chownSync("fs2.txt", ${uid}, ${gid});` +
                         'fs.unlinkSync("fs2.txt")';
tests['fs.sync.close'] = 'fs.writeFileSync("fs3.txt", "123", "utf8");' +
                         'fs.unlinkSync("fs3.txt")';
tests['fs.sync.copyfile'] = 'fs.writeFileSync("fs4.txt", "123", "utf8");' +
                            'fs.copyFileSync("fs4.txt","a.txt");' +
                            'fs.unlinkSync("fs4.txt")';
tests['fs.sync.fchmod'] = 'fs.writeFileSync("fs5.txt", "123", "utf8");' +
                          'const fd = fs.openSync("fs5.txt", "r+");' +
                          'fs.fchmodSync(fd,100);' +
                          'fs.unlinkSync("fs5.txt")';
tests['fs.sync.fchown'] = 'fs.writeFileSync("fs6.txt", "123", "utf8");' +
                          'const fd = fs.openSync("fs6.txt", "r+");' +
                          `fs.fchownSync(fd, ${uid}, ${gid});` +
                          'fs.unlinkSync("fs6.txt")';
tests['fs.sync.fdatasync'] = 'fs.writeFileSync("fs7.txt", "123", "utf8");' +
                             'const fd = fs.openSync("fs7.txt", "r+");' +
                             'fs.fdatasyncSync(fd);' +
                             'fs.unlinkSync("fs7.txt")';
tests['fs.sync.fstat'] = 'fs.writeFileSync("fs8.txt", "123", "utf8");' +
                         'fs.readFileSync("fs8.txt");' +
                         'fs.unlinkSync("fs8.txt")';
tests['fs.sync.fsync'] = 'fs.writeFileSync("fs9.txt", "123", "utf8");' +
                         'const fd = fs.openSync("fs9.txt", "r+");' +
                         'fs.fsyncSync(fd);' +
                         'fs.unlinkSync("fs9.txt")';
tests['fs.sync.ftruncate'] = 'fs.writeFileSync("fs10.txt", "123", "utf8");' +
                             'const fd = fs.openSync("fs10.txt", "r+");' +
                             'fs.ftruncateSync(fd, 1);' +
                             'fs.unlinkSync("fs10.txt")';
tests['fs.sync.futimes'] = 'fs.writeFileSync("fs11.txt", "123", "utf8");' +
                           'const fd = fs.openSync("fs11.txt", "r+");' +
                           'fs.futimesSync(fd,1,1);' +
                           'fs.unlinkSync("fs11.txt")';
tests['fs.sync.lchown'] = 'fs.writeFileSync("fs12.txt", "123", "utf8");' +
                          `fs.lchownSync("fs12.txt", ${uid}, ${gid});` +
                          'fs.unlinkSync("fs12.txt")';
tests['fs.sync.link'] = 'fs.writeFileSync("fs13.txt", "123", "utf8");' +
                        'fs.linkSync("fs13.txt", "fs14.txt");' +
                        'fs.unlinkSync("fs13.txt");' +
                        'fs.unlinkSync("fs14.txt")';
tests['fs.sync.lstat'] = 'fs.writeFileSync("fs15.txt", "123", "utf8");' +
                         'fs.lstatSync("fs15.txt");' +
                         'fs.unlinkSync("fs15.txt")';
tests['fs.sync.mkdir'] = 'fs.mkdirSync("fstemp0");' +
                         'fs.rmdirSync("fstemp0")';
tests['fs.sync.mkdtemp'] = 'const fp = fs.mkdtempSync("fstemp1");' +
                           'fs.rmdirSync(fp)';
tests['fs.sync.open'] = 'fs.writeFileSync("fs16.txt", "123", "utf8");' +
                        'fs.unlinkSync("fs16.txt")';
tests['fs.sync.read'] = 'fs.writeFileSync("fs17.txt", "123", "utf8");' +
                        'fs.readFileSync("fs17.txt");' +
                        'fs.unlinkSync("fs17.txt")';
tests['fs.sync.readdir'] = 'fs.readdirSync("./")';
tests['fs.sync.realpath'] = 'fs.writeFileSync("fs18.txt", "123", "utf8");' +
                            'fs.linkSync("fs18.txt", "fs19.txt");' +
                            'fs.realpathSync.native("fs19.txt");' +
                            'fs.unlinkSync("fs18.txt");' +
                            'fs.unlinkSync("fs19.txt")';
tests['fs.sync.rename'] = 'fs.writeFileSync("fs20.txt", "123", "utf8");' +
                          'fs.renameSync("fs20.txt","fs21.txt"); ' +
                          'fs.unlinkSync("fs21.txt")';
tests['fs.sync.rmdir'] = 'fs.mkdirSync("fstemp2");' +
                         'fs.rmdirSync("fstemp2")';
tests['fs.sync.stat'] = 'fs.writeFileSync("fs22.txt", "123", "utf8");' +
                        'fs.statSync("fs22.txt");' +
                        'fs.unlinkSync("fs22.txt")';
tests['fs.sync.unlink'] = 'fs.writeFileSync("fs23.txt", "123", "utf8");' +
                          'fs.linkSync("fs23.txt", "fs24.txt");' +
                          'fs.unlinkSync("fs23.txt");' +
                          'fs.unlinkSync("fs24.txt")';
tests['fs.sync.utimes'] = 'fs.writeFileSync("fs25.txt", "123", "utf8");' +
                          'fs.utimesSync("fs25.txt",1,1);' +
                          'fs.unlinkSync("fs25.txt")';
tests['fs.sync.write'] = 'fs.writeFileSync("fs26.txt", "123", "utf8");' +
                         'fs.unlinkSync("fs26.txt")';

// On windows, we need permissions to test symlink and readlink.
// We'll only try to run these tests if we have enough privileges.
if (common.canCreateSymLink()) {
  tests['fs.sync.symlink'] = 'fs.writeFileSync("fs27.txt", "123", "utf8");' +
                             'fs.symlinkSync("fs27.txt", "fs28.txt");' +
                             'fs.unlinkSync("fs27.txt");' +
                             'fs.unlinkSync("fs28.txt")';
  tests['fs.sync.readlink'] = 'fs.writeFileSync("fs29.txt", "123", "utf8");' +
                              'fs.symlinkSync("fs29.txt", "fs30.txt");' +
                              'fs.readlinkSync("fs30.txt");' +
                              'fs.unlinkSync("fs29.txt");' +
                              'fs.unlinkSync("fs30.txt")';
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const traceFile = tmpdir.resolve('node_trace.1.log');

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
