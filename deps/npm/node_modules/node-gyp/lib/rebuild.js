
module.exports = exports = rebuild

exports.usage = 'Runs "clean", "configure" and "build" all at once'

var log = require('npmlog')

function rebuild (gyp, argv, callback) {

  // first "clean"
  gyp.commands.clean([], function (err) {
    if (err) {
      // don't bail
      log.info('rebuild clean failed', err.stack);
    }

    gyp.commands.configure([], function (err) {
      if (err) return callback(err);
      gyp.commands.build([], callback);
    });
  });
}
