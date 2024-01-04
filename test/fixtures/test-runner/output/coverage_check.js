// Flags: --expose-internals --experimental-test-coverage --check-coverage --lines=100 --branches=100 --functions=100

'use strict';
require('../../../common');
const { TestCoverage } = require('internal/test_runner/coverage');
const { test, mock } = require('node:test');

mock.method(TestCoverage.prototype, 'summary', () => {
    return {
        files: [],
        totals: {
            totalLineCount: 100,
            totalBranchCount: 100,
            totalFunctionCount: 100,
            coveredLineCount: 100,
            coveredBranchCount: 100,
            coveredFunctionCount: 100,
            coveredLinePercent: 100,
            coveredBranchPercent: 100,
            coveredFunctionPercent: 100
        }
    }
});

test('ok');
