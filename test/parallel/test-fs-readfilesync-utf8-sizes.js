'use strict';
// fs.readFileSync(path, 'utf8') takes a dedicated native path. Its result must
// equal fs.readFileSync(path).toString('utf8') for every file size (in
// particular around its internal 8 KiB stack buffer and for multi-megabyte
// files), for file descriptors positioned mid-file, and for files whose
// reported size is wrong (procfs reports 0, sysfs reports a page).
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');

tmpdir.refresh();

function content(size) {
  // Multi-byte characters straddling every possible chunk boundary.
  const unit = 'abcdé€\u{1F600}\n';
  let s = unit.repeat(Math.ceil(size / unit.length));
  s = s.slice(0, size);
  // Avoid ending on a lone surrogate produced by slice().
  if (/[\ud800-\udbff]$/.test(s)) s = s.slice(0, -1) + 'x';
  return s;
}

const sizes = [0, 1, 8190, 8191, 8192, 8193, 8194, 16383, 16384, 16385,
               65535, 65536, 65537, 100000, (1 << 20) - 1, 1 << 20, (1 << 20) + 1,
               (8 << 20) + 5];
for (const size of sizes) {
  const file = tmpdir.resolve(`f-${size}.txt`);
  const str = content(size);
  fs.writeFileSync(file, str);
  const expected = fs.readFileSync(file).toString('utf8');
  assert.strictEqual(fs.readFileSync(file, 'utf8'), expected, `size ${size} by path`);
  assert.strictEqual(fs.readFileSync(file, { encoding: 'utf-8' }), expected, `size ${size} utf-8 alias`);
  // By fd: from the start (leaves the fd at EOF), then at EOF, then from a
  // mid-file position on a fresh fd.
  let fd = fs.openSync(file, 'r');
  try {
    assert.strictEqual(fs.readFileSync(fd, 'utf8'), expected, `size ${size} by fd`);
    assert.strictEqual(fs.readFileSync(fd, 'utf8'), '', `size ${size} by fd at EOF`);
  } finally {
    fs.closeSync(fd);
  }
  if (size > 10) {
    fd = fs.openSync(file, 'r');
    try {
      // Advance the fd 3 bytes (inside the ASCII prefix, so still valid UTF-8).
      assert.strictEqual(fs.readSync(fd, Buffer.alloc(3), 0, 3, null), 3);
      assert.strictEqual(fs.readFileSync(fd, 'utf8'), Buffer.from(expected).subarray(3).toString('utf8'),
                         `size ${size} by fd at offset 3`);
    } finally {
      fs.closeSync(fd);
    }
  }
}

// Binary garbage is decoded with replacement characters identically.
{
  const file = tmpdir.resolve('binary.bin');
  const buf = Buffer.alloc(20000);
  for (let i = 0; i < buf.length; i++) buf[i] = (i * 7919) & 0xff;
  fs.writeFileSync(file, buf);
  assert.strictEqual(fs.readFileSync(file, 'utf8'), buf.toString('utf8'));
}

// Files whose st_size does not describe their content.
if (common.isLinux) {
  for (const file of ['/proc/self/status', '/proc/self/smaps', '/proc/cpuinfo',
                      '/proc/version', '/sys/kernel/mm/transparent_hugepage/enabled']) {
    let viaBuffer;
    try {
      viaBuffer = fs.readFileSync(file);
    } catch {
      continue;  // Not available in this environment.
    }
    const viaUtf8 = fs.readFileSync(file, 'utf8');
    if (file !== '/proc/version' && file.startsWith('/proc/')) {
      // Content legitimately differs between two reads; compare shape instead.
      assert.ok(viaUtf8.length > 0);
      assert.strictEqual(viaUtf8.split('\n').length > 5, true, file);
      // Of these, smaps reliably exceeds the 8 KiB stack buffer.
      if (file === '/proc/self/smaps') assert.ok(viaUtf8.length > 8192, `smaps is only ${viaUtf8.length} chars`);
    } else {
      assert.strictEqual(viaUtf8, viaBuffer.toString('utf8'), file);
    }
  }
}

// Directory: same outcome either way (EISDIR, except on platforms where
// read() accepts directories, e.g. AIX).
function outcome(read) {
  try {
    return read();
  } catch (err) {
    return err.code;
  }
}
assert.strictEqual(outcome(() => fs.readFileSync(tmpdir.path, 'utf8')),
                   outcome(() => fs.readFileSync(tmpdir.path).toString('utf8')));
