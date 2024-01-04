// Flags: --expose-internals --experimental-test-coverage --check-coverage --lines=100 --branches=100 --functions=100

'use strict';
require('../../../common');
const { TestCoverage } = require('internal/test_runner/coverage');
const { test, mock } = require('node:test');

mock.method(TestCoverage.prototype, 'summary', () => {
    return {
        files: [],
        totals: {
            totalLineCount: 0,
            totalBranchCount: 0,
            totalFunctionCount: 0,
            coveredLineCount: 0,
            coveredBranchCount: 0,
            coveredFunctionCount: 0,
            coveredLinePercent: 0,
            coveredBranchPercent: 0,
            coveredFunctionPercent: 0
        }
    }
});

test('ok');
