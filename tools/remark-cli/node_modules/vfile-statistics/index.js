'use strict';

module.exports = statistics;

/* Get stats for a file, list of files, or list of messages. */
function statistics(files) {
  var result = {true: 0, false: 0, null: 0};

  count(files);

  return {
    fatal: result.true,
    nonfatal: result.false + result.null,
    warn: result.false,
    info: result.null,
    total: result.true + result.false + result.null
  };

  function count(value) {
    if (value) {
      /* Multiple vfiles */
      if (value[0] && value[0].messages) {
        countInAll(value);
      /* One vfile / messages */
      } else {
        countAll(value.messages || value);
      }
    }
  }

  function countInAll(files) {
    var length = files.length;
    var index = -1;

    while (++index < length) {
      count(files[index].messages);
    }
  }

  function countAll(messages) {
    var length = messages.length;
    var index = -1;
    var fatal;

    while (++index < length) {
      fatal = messages[index].fatal;
      result[fatal === null || fatal === undefined ? null : Boolean(fatal)]++;
    }
  }
}
