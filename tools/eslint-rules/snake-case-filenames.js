/**
 * @fileoverview enforces snake_case for filenames.
 * @author Shobhit Chittora <chittorashobhit@gmail.com>
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const path = require('path');

module.exports = function(context) {
  const snakeCaseRegex = new RegExp('^[a-z_]+$');
  const targetDirs = ['lib'];

  function checkIfInTargetDir(dirName) {
    return targetDirs.filter((dir) => dirName.indexOf(dir) !== -1).length !== 0;
  }

  return {
    'Program': function(node) {
      const filename = path.resolve(context.getFilename());
      const name = path.basename(filename, path.extname(filename));
      const dirName = path.dirname(filename);

      if (!snakeCaseRegex.test(name) && checkIfInTargetDir(dirName)) {
        context.report(node, "Filename '{{name}}' is not in snake_case.", {
          name
        });
      }
    }
  };
};
