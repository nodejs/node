/**
 * @fileoverview This is a file overview with no lines above it, at the top of
 *      the file (GOOD).
 */
/** // WRONG_BLANK_LINE_COUNT
 * @fileoverview This is a file overview with no lines above it (BAD).
 */

/**
 * @fileoverview This is a file overview with one line above it (GOOD).
 */


/**
 * @fileoverview This is a file overview with two lines above it (GOOD).
 */

/** // WRONG_BLANK_LINE_COUNT
 * A constructor with 1 line above it (BAD).
 * @constructor
 */
function someFunction() {}


/** // WRONG_BLANK_LINE_COUNT
 * A constructor with 2 lines above it (BAD).
 * @constructor
 */
function someFunction() {}



/**
 * A constructor with 3 lines above it (GOOD).
 * @constructor
 */
function someFunction() {}




/** // WRONG_BLANK_LINE_COUNT
 * A constructor with 4 lines above it (BAD).
 * @constructor
 */
function someFunction() {}

/** // WRONG_BLANK_LINE_COUNT
 * Top level block with 1 line above it (BAD).
 */
function someFunction() {}


/**
 * Top level block with 2 lines above it (GOOD).
 */
function someFunction() {}



/** // WRONG_BLANK_LINE_COUNT
 * Top level block with 3 lines above it (BAD).
 */
function someFunction() {}

 
// -1: EXTRA_SPACE
/**
 * Top level block with 2 lines above it, one contains whitespace (GOOD).
 */
function someFunction() {}


// This comment should be ignored.
/**
 * Top level block with 2 lines above it (GOOD).
 */
function someFunction() {}

// Should not check jsdocs which are inside a block.
var x = {
  /**
   * @constructor
   */
};

/**
 * This jsdoc-style comment should not be required to have two lines above it
 * since it does not immediately precede any code.
 */
// This is a comment.

/**
 * This jsdoc-style comment should not be required to have two lines above it
 * since it does not immediately precede any code.
 */
/**
 * This is a comment.
 */

/**
 * This jsdoc-style comment should not be required to have two lines above it
 * since it does not immediately precede any code.
 */
