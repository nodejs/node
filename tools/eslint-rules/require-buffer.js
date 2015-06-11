'use strict';

const msg = "Use const Buffer = require('buffer').Buffer; on top of this file";

module.exports = function(context) {
  return {
    "Program:exit": function() {
      context.getScope().through.forEach(function(ref) {
        if (ref.identifier.name === 'Buffer') {
            context.report(ref.identifier, msg);
        }
      });
    }
  }
}
