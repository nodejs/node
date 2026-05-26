// Fixture for test/parallel/test-assert-ok-source-maps.js.
// Run with --enable-source-maps; the file has no source map, so the error's
// source line resolution must fall back to the generated line instead of
// throwing ERR_INVALID_ARG_TYPE (regression test for
// https://github.com/nodejs/node/issues/63169).
import { strict as assert } from 'node:assert';

try {
  assert(false);
} catch (e) {
  process.stdout.write(e.constructor.name + '\n');
  process.stdout.write(e.code + '\n');
  process.stdout.write(e.message + '\n');
  process.exit(0);
}
process.exit(1);
