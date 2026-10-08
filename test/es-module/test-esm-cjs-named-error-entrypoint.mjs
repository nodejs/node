import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'assert';

const entryPoint = fixtures.path('es-modules', 'package-cjs-named-error', 'single-quote.mjs');
spawnSyncAndAssert(process.execPath, [entryPoint], {
  status: 1,
  stderr(stderr) {
    assert(stderr.includes("Named export 'comeOn' not found."),
           'entry point should show the missing named export');
    assert(stderr.includes('CommonJS modules can always be imported via the default export'),
           'entry point should show the CommonJS named export hint');
    assert(stderr.includes("import pkg from './fail.cjs';"),
           'entry point hint should recommend the default import');
    assert(stderr.includes('const { comeOn } = pkg;'),
           'entry point hint should show the named import as destructuring');
    assert(stderr.includes("import { comeOn } from './fail.cjs';"),
           'entry point error should include the source import statement');
  },
});
