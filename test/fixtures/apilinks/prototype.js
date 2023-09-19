'use strict';

// An exported class using classic prototype syntax.

function Class() {
}

Class.classMethod = function() {}
Class.prototype.instanceMethod = function() {}

module.exports = {
  Class
};
