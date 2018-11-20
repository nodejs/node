var assert = require('assert');
assert.notEqual(module, require.main, 'require.main should not == module');
assert.notEqual(module, process.mainModule,
                'process.mainModule should not === module');
