// Flags: --experimental-loader ./test/fixtures/es-module-loaders/hooks-custom.mjs
import { mustCall } from '../common/index.mjs';
import assert from 'assert';

const done = mustCall(() => { assert.ok(true); });


// Test that the process doesn't exit because of a caught exception thrown as part of dynamic import().
await import('nonexistent/file.mjs').catch(() => {});
await import('nonexistent/file.mjs').catch(() => {});
await import('nonexistent/file.mjs').catch(() => {});
await import('nonexistent/file.mjs').catch(() => {});
await import('nonexistent/file.mjs').catch(() => {});
await import('nonexistent/file.mjs').catch(() => {});

done();
