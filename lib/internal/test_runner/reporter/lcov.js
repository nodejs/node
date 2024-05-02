'use strict';

const { relative } = require('path');
const Transform = require('internal/streams/transform');

// This reporter is based on the LCOV format, as described here:
// https://ltp.sourceforge.net/coverage/lcov/geninfo.1.php
// Excerpts from this documentation are included in the comments that make up
// the _transform function below.
class LcovReporter extends Transform {
  constructor(options) {
    super({ ...options, writableObjectMode: true, __proto__: null });
  }

  _transform(event, _encoding, callback) {
    if (event.type !== 'test:coverage') {
      return callback(null);
    }
    let lcov = '';
    // A tracefile is made up of several human-readable lines of text, divided
    // into sections. If available, a tracefile begins with the testname which
    // is stored in the following format:
    // ## TN:\<test name\>
    lcov += 'TN:\n';
    const {
      data: {
        summary: { workingDirectory },
      },
    } = event;
    try {
      for (let i = 0; i < event.data.summary.files.length; i++) {
        const file = event.data.summary.files[i];
        // For each source file referenced in the .da file, there is a section
        // containing filename and coverage data:
        // ## SF:\<path to the source file\>
        lcov += `SF:${relative(workingDirectory, file.path)}\n`;

        // Following is a list of line numbers for each function name found in
        // the source file:
        // ## FN:\<line number of function start\>,\<function name\>
        //
        // After, there is a list of execution counts for each instrumented
        // function:
        // ## FNDA:\<execution count\>,\<function name\>
        //
        // This loop adds the FN lines to the lcov variable as it goes and
        // gathers the FNDA lines to be added later. This way we only loop
        // through the list of functions once.
        let fnda = '';
        for (let j = 0; j < file.functions.length; j++) {
          const func = file.functions[j];
          const name = func.name || `anonymous_${j}`;
          lcov += `FN:${func.line},${name}\n`;
          fnda += `FNDA:${func.count},${name}\n`;
        }
        lcov += fnda;

        // This list is followed by two lines containing the number of
        // functions found and hit:
        // ## FNF:\<number of functions found\>
        // ## FNH:\<number of function hit\>
        lcov += `FNF:${file.totalFunctionCount}\n`;
        lcov += `FNH:${file.coveredFunctionCount}\n`;

        // Branch coverage information is stored which one line per branch:
        // ## BRDA:\<line number\>,\<block number\>,\<branch number\>,\<taken\>
        // Block number and branch number are gcc internal IDs for the branch.
        // Taken is either '-' if the basic block containing the branch was
        // never executed or a number indicating how often that branch was
        // taken.
        for (let j = 0; j < file.branches.length; j++) {
          lcov += `BRDA:${file.branches[j].line},${j},0,${file.branches[j].count}\n`;
        }

        // Branch coverage summaries are stored in two lines:
        // ## BRF:\<number of branches found\>
        // ## BRH:\<number of branches hit\>
        lcov += `BRF:${file.totalBranchCount}\n`;
        lcov += `BRH:${file.coveredBranchCount}\n`;

        // Then there is a list of execution counts for each instrumented line
        // (i.e. a line which resulted in executable code):
        // ## DA:\<line number\>,\<execution count\>[,\<checksum\>]
        const sortedLines = file.lines.toSorted((a, b) => a.line - b.line);
        for (let j = 0; j < sortedLines.length; j++) {
          lcov += `DA:${sortedLines[j].line},${sortedLines[j].count}\n`;
        }

        // At the end of a section, there is a summary about how many lines
        // were found and how many were actually instrumented:
        // ## LH:\<number of lines with a non-zero execution count\>
        // ## LF:\<number of instrumented lines\>
        lcov += `LH:${file.coveredLineCount}\n`;
        lcov += `LF:${file.totalLineCount}\n`;

        // Each sections ends with:
        // end_of_record
        lcov += 'end_of_record\n';
      }
    } catch (error) {
      return callback(error);
    }
    return callback(null, lcov);
  }
}

module.exports = LcovReporter;
