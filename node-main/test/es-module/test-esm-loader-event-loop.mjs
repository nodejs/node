// Flags: --experimental-loader ./test/fixtures/es-module-loaders/hooks-custom.mjs
import { mustCall } from '../common/index.mjs';

const done = mustCall();


// Test that the process doesn't exit because of a caught exception thrown as part of dynamic import().
for (let i = 0; i < 10; i++) {
  await import('nonexistent/file.mjs').catch(() => {});
}

done();
