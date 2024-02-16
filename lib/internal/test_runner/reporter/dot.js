'use strict';

// Importing required modules
const { MathMax } = primordials;
const { green, red } = require('internal/util/colors');

// Exporting the dot reporter function as an asynchronous generator
module.exports = async function* dot(source) {
  // Initializing count and columns variables
  let count = 0;
  let columns = getLineLength();

  // Iterating over the test events from the source
  for await (const { type } of source) {
    // Check if the test passed
    if (type === 'test:pass') {
      // Print a green dot for pass
      yield green('.');
    }

    // Check if the test failed
    if (type === 'test:fail') {
      // Print a red X for fail
      yield red('X');
    }

    // Check if it's time to move to a new line
    if ((type === 'test:fail' || type === 'test:pass') && ++count === columns) {
      // Print a newline character

      // Getting the line length again in case the terminal was resized.
      columns = getLineLength();
      count = 0;
    }
  }

  // Print a newline character at the end
  yield '\n';
};

// Function to get the line length for formatting
function getLineLength() {
  // Use the greater of the terminal columns or a default of 20 columns
  return MathMax(process.stdout.columns ?? 20, 20);
}
