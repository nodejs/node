'use strict';

var wrapped = require('wrapped');

module.exports = factory;

function factory(id, rule) {
  var parts = id.split(':');
  var source = parts[0];
  var ruleId = parts[1];
  var fn = wrapped(rule);

  /* istanbul ignore if - possibly useful if externalised later. */
  if (!ruleId) {
    ruleId = source;
    source = null;
  }

  attacher.displayName = id;

  return attacher;

  function attacher(raw) {
    var config = coerce(ruleId, raw);
    var severity = config[0];
    var options = config[1];
    var fatal = severity === 2;

    return severity ? transformer : undefined;

    function transformer(tree, file, next) {
      var index = file.messages.length;

      fn(tree, file, options, done);

      function done(err) {
        var messages = file.messages;
        var message;

        /* Add the error, if not already properly added. */
        /* istanbul ignore if - only happens for incorrect plugins */
        if (err && messages.indexOf(err) === -1) {
          try {
            file.fail(err);
          } catch (err) {}
        }

        while (index < messages.length) {
          message = messages[index];
          message.ruleId = ruleId;
          message.source = source;
          message.fatal = fatal;

          index++;
        }

        next();
      }
    }
  }
}

/* Coerce a value to a severity--options tuple. */
function coerce(name, value) {
  var def = 1;
  var result;
  var level;

  /* istanbul ignore if - Handled by unified in v6.0.0 */
  if (typeof value === 'boolean') {
    result = [value];
  } else if (value == null) {
    result = [def];
  } else if (
    typeof value === 'object' &&
    (
      typeof value[0] === 'number' ||
      typeof value[0] === 'boolean' ||
      typeof value[0] === 'string'
    )
  ) {
    result = value.concat();
  } else {
    result = [1, value];
  }

  level = result[0];

  if (typeof level === 'boolean') {
    level = level ? 1 : 0;
  } else if (typeof level === 'string') {
    if (level === 'off') {
      level = 0;
    } else if (level === 'on' || level === 'warn') {
      level = 1;
    } else if (level === 'error') {
      level = 2;
    } else {
      level = 1;
      result = [level, result];
    }
  }

  if (level < 0 || level > 2) {
    throw new Error(
      'Invalid severity `' + level + '` for `' + name + '`, ' +
      'expected 0, 1, or 2'
    );
  }

  result[0] = level;

  return result;
}
