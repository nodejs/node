'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const tests = new Array();
const traceFile = 'node_trace.1.log';

let gid = 1;
let uid = 1;
let skipSymlinks = false;

// On Windows, creating symlinks requires admin privileges.
// We'll check if we have enough privileges.
if (common.isWindows) {
  try {
    const o = cp.execSync('whoami /priv');
    if (!o.includes('SeCreateSymbolicLinkPrivilege')) {
      skipSymlinks = true;
    }
  } catch (er) {
    // better safe than sorry
    skipSymlinks = true;
  }
} else {
  gid = process.getgid();
  uid = process.getuid();
}

tests['fs.sync.access'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'fs.accessSync("fs.txt");' +
                          'fs.unlinkSync("fs.txt")';
tests['fs.sync.chmod'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'fs.chmodSync("fs.txt",100);' +
                         'fs.unlinkSync("fs.txt")';
tests['fs.sync.chown'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'fs.chownSync("fs.txt",' + uid + ',' + gid + ');' +
                         'fs.unlinkSync("fs.txt")';
tests['fs.sync.close'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'fs.unlinkSync("fs.txt")';
tests['fs.sync.copyfile'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                            'fs.copyFileSync("fs.txt","a.txt");' +
                            'fs.unlinkSync("fs.txt")';
tests['fs.sync.fchmod'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'const fd = fs.openSync("fs.txt", "r+");' +
                          'fs.fchmodSync(fd,100);' +
                          'fs.unlinkSync("fs.txt")';
tests['fs.sync.fchown'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'const fd = fs.openSync("fs.txt", "r+");' +
                          'fs.fchownSync(fd,' + uid + ',' + gid + ');' +
                          'fs.unlinkSync("fs.txt")';
tests['fs.sync.fdatasync'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                             'const fd = fs.openSync("fs.txt", "r+");' +
                             'fs.fdatasyncSync(fd);' +
                             'fs.unlinkSync("fs.txt")';
tests['fs.sync.fstat'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'fs.readFileSync("fs.txt");' +
                         'fs.unlinkSync("fs.txt")';
tests['fs.sync.fsync'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'const fd = fs.openSync("fs.txt", "r+");' +
                         'fs.fsyncSync(fd);' +
                         'fs.unlinkSync("fs.txt")';
tests['fs.sync.ftruncate'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                             'const fd = fs.openSync("fs.txt", "r+");' +
                             'fs.ftruncateSync(fd, 1);' +
                             'fs.unlinkSync("fs.txt")';
tests['fs.sync.futimes'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                           'const fd = fs.openSync("fs.txt", "r+");' +
                           'fs.futimesSync(fd,1,1);' +
                           'fs.unlinkSync("fs.txt")';
tests['fs.sync.link'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'fs.linkSync("fs.txt", "linkx");' +
                        'fs.unlinkSync("linkx");' +
                        'fs.unlinkSync("fs.txt")';
tests['fs.sync.lstat'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'fs.lstatSync("fs.txt");' +
                         'fs.unlinkSync("fs.txt")';
tests['fs.sync.mkdir'] = 'fs.mkdirSync("fstemp");' +
                         'fs.rmdirSync("fstemp")';
tests['fs.sync.mkdtemp'] = 'const fp = fs.mkdtempSync("fstest");' +
                           'fs.rmdirSync(fp)';
tests['fs.sync.open'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'fs.unlinkSync("fs.txt")';
tests['fs.sync.read'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'fs.readFileSync("fs.txt");' +
                        'fs.unlinkSync("fs.txt")';
tests['fs.sync.readdir'] = 'fs.readdirSync("./")';
tests['fs.sync.realpath'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                            'fs.linkSync("fs.txt", "linkx");' +
                            'fs.realpathSync.native("linkx");' +
                            'fs.unlinkSync("linkx");' +
                            'fs.unlinkSync("fs.txt")';
tests['fs.sync.rename'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'fs.renameSync("fs.txt","xyz.txt"); ' +
                          'fs.unlinkSync("xyz.txt")';
tests['fs.sync.rmdir'] = 'fs.mkdirSync("fstemp");' +
                         'fs.rmdirSync("fstemp")';
tests['fs.sync.stat'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                        'fs.statSync("fs.txt");' +
                        'fs.unlinkSync("fs.txt")';
tests['fs.sync.unlink'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'fs.linkSync("fs.txt", "linkx");' +
                          'fs.unlinkSync("linkx");' +
                          'fs.unlinkSync("fs.txt")';
tests['fs.sync.utimes'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                          'fs.utimesSync("fs.txt",1,1);' +
                          'fs.unlinkSync("fs.txt")';
tests['fs.sync.write'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                         'fs.unlinkSync("fs.txt")';

// On windows, we need permissions to test symlink and readlink.
// We'll only try to run these tests if we have enough privileges.
if (!skipSymlinks) {
  tests['fs.sync.symlink'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                             'fs.symlinkSync("fs.txt", "linkx");' +
                             'fs.unlinkSync("linkx");' +
                             'fs.unlinkSync("fs.txt")';
  tests['fs.sync.readlink'] = 'fs.writeFileSync("fs.txt", "123", "utf8");' +
                              'fs.symlinkSync("fs.txt", "linkx");' +
                              'fs.readlinkSync("linkx");' +
                              'fs.unlinkSync("linkx");' +
                              'fs.unlinkSync("fs.txt")';
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
process.chdir(tmpdir.path);

for (const tr in tests) {
  const proc = cp.spawnSync(process.execPath,
                            [ '--trace-events-enabled',
                              '--trace-event-categories', 'node.fs.sync',
                              '-e', tests[tr] ]);
  // Some AIX versions don't support futimes or utimes, so skip.
  if (common.isAIX && proc.status !== 0 && tr === 'fs.sync.futimes') {
    continue;
  }
  if (common.isAIX && proc.status !== 0 && tr === 'fs.sync.utimes') {
    continue;
  }

  // Make sure the operation is successful.
  assert.strictEqual(proc.status, 0, tr + ': ' + proc.stderr);

  // Confirm that trace log file is created.
  assert(common.fileExists(traceFile));
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
