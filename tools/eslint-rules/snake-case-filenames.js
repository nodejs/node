/**
 * @fileoverview enforces snake_case for filenames.
 * @author Shobhit Chittora <chittorashobhit@gmail.com>
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
  create: function(context) {
    return {
      'Program:exit': function(node) {
        var filename = context.getFilename();
        context.report(node, `Filename ${filename} does 
                              not match the naming convention.`);
      }
    };
  }
};
