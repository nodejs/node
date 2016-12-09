'use strict';
const common = require('../common');
const assert = require('assert');
const os = require('os');
const path = require('path');

const is = {
  string: (value) => { assert.strictEqual(typeof value, 'string'); },
  number: (value) => { assert.strictEqual(typeof value, 'number'); },
  array: (value) => { assert.ok(Array.isArray(value)); },
  object: (value) => {
    assert.strictEqual(typeof value, 'object');
    assert.notStrictEqual(value, null);
  }
};

process.env.TMPDIR = '/tmpdir';
process.env.TMP = '/tmp';
process.env.TEMP = '/temp';
if (common.isWindows) {
  assert.strictEqual(os.tmpdir(), '/temp');
  process.env.TEMP = '';
  assert.strictEqual(os.tmpdir(), '/tmp');
  process.env.TMP = '';
  const expected = (process.env.SystemRoot || process.env.windir) + '\\temp';
  assert.strictEqual(os.tmpdir(), expected);
  process.env.TEMP = '\\temp\\';
  assert.strictEqual(os.tmpdir(), '\\temp');
  process.env.TEMP = '\\tmpdir/';
  assert.strictEqual(os.tmpdir(), '\\tmpdir/');
  process.env.TEMP = '\\';
  assert.strictEqual(os.tmpdir(), '\\');
  process.env.TEMP = 'C:\\';
  assert.strictEqual(os.tmpdir(), 'C:\\');
} else {
  assert.strictEqual(os.tmpdir(), '/tmpdir');
  process.env.TMPDIR = '';
  assert.strictEqual(os.tmpdir(), '/tmp');
  process.env.TMP = '';
  assert.strictEqual(os.tmpdir(), '/temp');
  process.env.TEMP = '';
  assert.strictEqual(os.tmpdir(), '/tmp');
  process.env.TMPDIR = '/tmpdir/';
  assert.strictEqual(os.tmpdir(), '/tmpdir');
  process.env.TMPDIR = '/tmpdir\\';
  assert.strictEqual(os.tmpdir(), '/tmpdir\\');
  process.env.TMPDIR = '/';
  assert.strictEqual(os.tmpdir(), '/');
}

const endianness = os.endianness();
console.log('endianness = %s', endianness);
is.string(endianness);
assert.ok(/[BL]E/.test(endianness));

const hostname = os.hostname();
console.log('hostname = %s', hostname);
is.string(hostname);
assert.ok(hostname.length > 0);

const uptime = os.uptime();
console.log('uptime = %d', uptime);
is.number(uptime);
assert.ok(uptime > 0);

const cpus = os.cpus();
console.log('cpus = ', cpus);
is.array(cpus);
assert.ok(cpus.length > 0);

const type = os.type();
console.log('type = ', type);
is.string(type);
assert.ok(type.length > 0);

const release = os.release();
console.log('release = ', release);
is.string(release);
assert.ok(release.length > 0);

const platform = os.platform();
console.log('platform = ', platform);
is.string(platform);
assert.ok(platform.length > 0);

const arch = os.arch();
console.log('arch = ', arch);
is.string(arch);
assert.ok(arch.length > 0);

if (!common.isSunOS) {
  // not implemeneted yet
  assert.ok(os.loadavg().length > 0);
  assert.ok(os.freemem() > 0);
  assert.ok(os.totalmem() > 0);
}


const interfaces = os.networkInterfaces();
console.error(interfaces);
switch (platform) {
  case 'linux':
    {
      const filter = function(e) { return e.address === '127.0.0.1'; };
      const actual = interfaces.lo.filter(filter);
      const expected = [{ address: '127.0.0.1', netmask: '255.0.0.0',
                        mac: '00:00:00:00:00:00', family: 'IPv4',
                        internal: true }];
      assert.deepStrictEqual(actual, expected);
      break;
    }
  case 'win32':
    {
      const filter = function(e) { return e.address === '127.0.0.1'; };
      const actual = interfaces['Loopback Pseudo-Interface 1'].filter(filter);
      const expected = [{ address: '127.0.0.1', netmask: '255.0.0.0',
                        mac: '00:00:00:00:00:00', family: 'IPv4',
                        internal: true }];
      assert.deepStrictEqual(actual, expected);
      break;
    }
}

const EOL = os.EOL;
assert.ok(EOL.length > 0);


const home = os.homedir();

console.log('homedir = ' + home);
is.string(home);
assert.ok(home.includes(path.sep));

if (common.isWindows && process.env.USERPROFILE) {
  assert.strictEqual(home, process.env.USERPROFILE);
  delete process.env.USERPROFILE;
  assert.ok(os.homedir().includes(path.sep));
  process.env.USERPROFILE = home;
} else if (!common.isWindows && process.env.HOME) {
  assert.strictEqual(home, process.env.HOME);
  delete process.env.HOME;
  assert.ok(os.homedir().includes(path.sep));
  process.env.HOME = home;
}

const pwd = os.userInfo();
is.object(pwd);
const pwdBuf = os.userInfo({ encoding: 'buffer' });

if (common.isWindows) {
  assert.strictEqual(pwd.uid, -1);
  assert.strictEqual(pwd.gid, -1);
  assert.strictEqual(pwd.shell, null);
  assert.strictEqual(pwdBuf.uid, -1);
  assert.strictEqual(pwdBuf.gid, -1);
  assert.strictEqual(pwdBuf.shell, null);
} else {
  is.number(pwd.uid);
  is.number(pwd.gid);
  assert.ok(pwd.shell.includes(path.sep));
  assert.strictEqual(pwd.uid, pwdBuf.uid);
  assert.strictEqual(pwd.gid, pwdBuf.gid);
  assert.strictEqual(pwd.shell, pwdBuf.shell.toString('utf8'));
}

is.string(pwd.username);
assert.ok(pwd.homedir.includes(path.sep));
assert.strictEqual(pwd.username, pwdBuf.username.toString('utf8'));
assert.strictEqual(pwd.homedir, pwdBuf.homedir.toString('utf8'));
