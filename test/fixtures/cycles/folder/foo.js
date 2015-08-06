const root = require('./../root');

exports.hello = function() {
  return root.calledFromFoo();
};
