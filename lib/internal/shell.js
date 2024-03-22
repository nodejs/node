'use strict';


// There is no need to add primordials to this file.
// `shell.js` is a script only executed when `node run <script>` is called.

const forbiddenCharacters = /[\t\n\r "#$&'()*;<>?\\`|~]/;

/**
 * Escapes a string to be used as a shell argument.
 *
 * Adapted from `promise-spawn` module available under ISC license.
 * Ref: https://github.com/npm/promise-spawn/blob/16b36410f9b721dbe190141136432a418869734f/lib/escape.js
 * @param {string} input
 */
function escapeShell(input) {
  // If the input is an empty string, return a pair of quotes
  if (!input.length) {
    return '\'\'';
  }

  // Check if input contains any forbidden characters
  // If it doesn't, return the input as is.
  if (!forbiddenCharacters.test(input)) {
    return input;
  }

  // Replace single quotes with '\'' and wrap the whole result in a fresh set of quotes
  return `'${input.replace(/'/g, '\'\\\'\'')}'`
    // If the input string already had single quotes around it, clean those up
    .replace(/^(?:'')+(?!$)/, '')
    .replace(/\\'''/g, '\\\'');
}

module.exports = {
  escapeShell,
};
