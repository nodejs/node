const assert = require('assert');
assert.notStrictEqual(module, require.main, 'require.main should not == module');
assert.notStrictEqual(module, process.mainModule,
                      'process.mainModule should not === module');
