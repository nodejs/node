const test = require('node:test');

// 'as string' ensures that type stripping actually occurs
test('typescript-test.spec.cts this should pass' as string);
