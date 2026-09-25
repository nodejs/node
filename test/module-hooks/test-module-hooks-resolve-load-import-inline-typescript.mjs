// Flags: --no-experimental-strip-types
// This tests that a mini TypeScript loader works with resolve and
// load hooks when TypeScript support is disabled.
import '../common/index.mjs';
import assert from 'node:assert';

await import('../fixtures/module-hooks/register-typescript-hooks.js');
// Test inline import(), if override fails, this should fail too because enum is
// not supported when type stripping is used without transformation.
const { UserAccount, UserType } = await import('../fixtures/module-hooks/user.ts');
assert.strictEqual((new UserAccount('foo', 1, UserType.Admin).name), 'foo');
