'use strict';
require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');
const { getCoverageReport } = require('internal/test_runner/utils');

/**
 *
 * Builds a coverage summary compatible object given an input
 *
 */
function buildCoverageSummary(input, workingDirectory = '/a/random/path') {
  const fixture = {
    workingDirectory: workingDirectory, // This can be adjusted as needed
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
      coveredFunctionPercent: 0,
    }
  };

  input.forEach(({ filename, uncoveredLines, coveredBranchPercent, coveredFunctionPercent, coveredLinePercent }) => {
    // Calculate total lines and covered lines
    const totalLineCount = Math.round(100 * uncoveredLines / (100 - coveredLinePercent));
    const coveredLineCount = totalLineCount - uncoveredLines;

    // Calculate total branches based on covered branch percentage
    const totalBranchCount = coveredBranchPercent === 100 ? 1 : Math.round(100 / coveredBranchPercent);
    const coveredBranchCount = Math.round(totalBranchCount * (coveredBranchPercent / 100));

    // Functions data
    const totalFunctionCount = coveredFunctionPercent === 100 ? 1 : 1;
    const coveredFunctionCount = coveredFunctionPercent === 100 ? 1 : 0;

    // Create the lines array with uncovered blocks
    const lines = [];
    for (let i = 0; i < uncoveredLines * 2; i += 2) {
      lines.push({
        line: 2 * i + 1,
        count: 0
      });
      lines.push({
        line: 2 * i + 2,
        count: 0
      });
    }

    // Add the covered lines after the uncovered blocks
    for (let i = uncoveredLines * 2; i < totalLineCount; i++) {
      lines.push({
        line: i + 1,
        count: 1 // Covered line
      });
    }

    // Update fixture files
    fixture.files.push({
      path: `${workingDirectory}/${filename}`,
      totalLineCount,
      totalBranchCount,
      totalFunctionCount,
      coveredLineCount,
      coveredBranchCount,
      coveredFunctionCount,
      coveredLinePercent,
      coveredBranchPercent,
      coveredFunctionPercent,
      functions: totalFunctionCount ? [{ name: '', count: 1, line: 1 }] : [],
      branches: Array(totalBranchCount).fill(null).map((_, i) => ({
        line: i + 1,
        count: i < coveredBranchCount ? 1 : 0,
      })),
      lines
    });

    // // Update totals
    fixture.totals.totalLineCount += totalLineCount;
    fixture.totals.totalBranchCount += totalBranchCount;
    fixture.totals.totalFunctionCount += totalFunctionCount;
    fixture.totals.coveredLineCount += coveredLineCount;
    fixture.totals.coveredBranchCount += coveredBranchCount;
    fixture.totals.coveredFunctionCount += coveredFunctionCount;
  });

  // Calculate total percentages
  fixture.totals.coveredLinePercent =
    (
      fixture.totals.coveredLineCount /
      fixture.totals.totalLineCount
    ) * 100;
  fixture.totals.coveredBranchPercent =
    (
      fixture.totals.coveredBranchCount /
      fixture.totals.totalBranchCount
    ) * 100;
  fixture.totals.coveredFunctionPercent =
    (
      fixture.totals.coveredFunctionCount /
      fixture.totals.totalFunctionCount
    ) * 100;

  return fixture;
}

function createCoverageTest({ testDescription, input, expectedResult, debug = false }) {
  it(testDescription, () => {
    const summary = buildCoverageSummary(input);

    const result = getCoverageReport(
      pad,
      summary,
      symbol,
      color,
      table
    );

    if (debug) {
      console.log(result);
    }
    assert.strictEqual(result, expectedResult);
  });
}

const pad = '';
const symbol = '# ';
const color = '';
const table = true;

describe('getCoverageReport', { concurrency: false }, () => {

  describe('Infinite Width', () => {
    createCoverageTest({
      testDescription: 'should print the coverage report for normal input',
      input: [
        {
          filename: 'a.test.ts',
          uncoveredLines: 6,
          coveredBranchPercent: 50,
          coveredFunctionPercent: 100,
          coveredLinePercent: 53.80
        },
        {
          filename: 'b.test.ts',
          uncoveredLines: 4,
          coveredBranchPercent: 100,
          coveredFunctionPercent: 0,
          coveredLinePercent: 55.55
        },
      ],
      expectedResult:
      `# start of coverage report
# -------------------------------------------------------------------------
# file      | line % | branch % | funcs % | uncovered lines
# -------------------------------------------------------------------------
# a.test.ts |  53.80 |    50.00 |  100.00 | 1-2 5-6 9-10 13-14 17-18 21-22
# b.test.ts |  55.55 |   100.00 |    0.00 | 1-2 5-6 9-10 13-14
# -------------------------------------------------------------------------
# all files |  54.55 |    66.67 |   50.00 |
# -------------------------------------------------------------------------
# end of coverage report
`
    });

    createCoverageTest({
      testDescription: 'should print the coverage report with a lot of uncovered lines in b.test.ts',
      input: [
        {
          filename: 'a.test.ts',
          uncoveredLines: 1,
          coveredBranchPercent: 50,
          coveredFunctionPercent: 100,
          coveredLinePercent: 53.80
        },
        {
          filename: 'b.test.ts',
          uncoveredLines: 2, // A large number of uncovered lines
          coveredBranchPercent: 100,
          coveredFunctionPercent: 0,
          coveredLinePercent: 10.00 // Only 10% of the lines are covered
        },
      ],
      expectedResult:
      `# start of coverage report
# ----------------------------------------------------------
# file      | line % | branch % | funcs % | uncovered lines
# ----------------------------------------------------------
# a.test.ts |  53.80 |    50.00 |  100.00 | 1-2
# b.test.ts |  10.00 |   100.00 |    0.00 | 1-2 5-6
# ----------------------------------------------------------
# all files |  25.00 |    66.67 |   50.00 |
# ----------------------------------------------------------
# end of coverage report
`
    });
  });

  describe('Limited Width - 80', () => {
    // Set the terminal width to 80
    process.stdout.columns = 80;

    createCoverageTest({
      testDescription: 'should print the coverage report with a lot of uncovered lines and very long filenames',
      input: [
        {
          filename: 'a/very/long/long/long/path/a.test.ts',
          uncoveredLines: 40,
          coveredBranchPercent: 50,
          coveredFunctionPercent: 100,
          coveredLinePercent: 53.80
        },
        {
          filename: 'a/very/long/long/long/path/b.test.ts',
          uncoveredLines: 4,
          coveredBranchPercent: 100,
          coveredFunctionPercent: 0,
          coveredLinePercent: 55.55
        },
      ],
      expectedResult:
      `# start of coverage report
# ------------------------------------------------------------------------------
# file     | line % | branch % | funcs % | uncovered lines
# ------------------------------------------------------------------------------
# …test.ts |  53.80 |    50.00 |  100.00 | 1-2 5-6 9-10 13-14 17-18 21-22 25-2…
# …test.ts |  55.55 |   100.00 |    0.00 | 1-2 5-6 9-10 13-14
# ------------------------------------------------------------------------------
# all fil… |  54.17 |    66.67 |   50.00 |
# ------------------------------------------------------------------------------
# end of coverage report
`
    });
  });
});
