// This tests that cp() into a destination that did not exist when it was
// checked, but does by the time the copy starts, fails with EEXIST instead
// of copying through whatever appeared there.
import '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { createHook } from 'node:async_hooks';
import { existsSync, lstatSync, mkdirSync, symlinkSync, writeFileSync, promises } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();
const src = nextdir();
const dest = nextdir();
const target = nextdir();
mkdirSync(src);
mkdirSync(target);
writeFileSync(join(src, 'file'), 'x');

let injected = false;
const hook = createHook({
  init(id, type) {
    if (!injected && type === 'FSREQCALLBACK' && !existsSync(dest)) {
      injected = true;
      symlinkSync(target, dest, 'dir');
    }
  },
}).enable();
const outcome = await promises.cp(src, dest, { recursive: true }).then(() => null, (err) => err);
hook.disable();
assert.ok(injected, 'the hook found no request to inject the symbolic link at');
assert.strictEqual(outcome?.code, 'EEXIST');
assert.strictEqual(outcome?.syscall, 'mkdir');
assert.ok(lstatSync(dest).isSymbolicLink());
assert.ok(!existsSync(join(target, 'file')));
