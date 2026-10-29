'use strict';
// This tests that --build-sea emits an ELF whose PT_LOAD segments never share
// a page, since kernels that map them with MAP_FIXED_NOREPLACE (Linux < 5.4,
// RHEL 8) refuse to execute such a file.

const common = require('../common');
if (process.platform !== 'linux')
  common.skip('ELF-specific');

const { buildSEA, skipIfBuildSEAIsNotSupported } = require('../common/sea');
skipIfBuildSEAIsNotSupported();

const assert = require('assert');
const { readFileSync } = require('fs');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const elf = readFileSync(buildSEA(fixtures.path('sea', 'basic')));
assert.strictEqual(elf[4], 2);  // ELFCLASS64
const phoff = Number(elf.readBigUInt64LE(0x20));
const phentsize = elf.readUInt16LE(0x36);
const phnum = elf.readUInt16LE(0x38);
const loads = [];
for (let i = 0; i < phnum; i++) {
  const at = phoff + i * phentsize;
  if (elf.readUInt32LE(at) !== 1) continue;  // PT_LOAD
  const vaddr = elf.readBigUInt64LE(at + 0x10);
  const memsz = elf.readBigUInt64LE(at + 0x28);
  if (memsz > 0n) loads.push({ first: vaddr >> 12n, last: (vaddr + memsz - 1n) >> 12n });
}
loads.sort((a, b) => (a.first < b.first ? -1 : 1));
for (let i = 1; i < loads.length; i++) {
  assert(loads[i].first > loads[i - 1].last,
         `PT_LOAD ${i} starts on page 0x${loads[i].first.toString(16)}, ` +
         `which PT_LOAD ${i - 1} already covers`);
}
