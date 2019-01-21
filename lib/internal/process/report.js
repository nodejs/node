'use strict';

const { emitExperimentalWarning } = require('internal/util');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_SYNTHETIC } = require('internal/errors').codes;

exports.setup = function() {
  const REPORTEVENTS = 1;
  const REPORTSIGNAL = 2;
  const REPORTFILENAME = 3;
  const REPORTPATH = 4;
  const REPORTVERBOSE = 5;

  // If report is enabled, extract the binding and
  // wrap the APIs with thin layers, with some error checks.
  // user options can come in from CLI / ENV / API.
  // CLI and ENV is intercepted in C++ and the API call here (JS).
  // So sync up with both sides as appropriate - initially from
  // C++ to JS and from JS to C++ whenever the API is called.
  // Some events are controlled purely from JS (signal | exception)
  // and some from C++ (fatalerror) so this sync-up is essential for
  // correct behavior and alignment with the supplied tunables.
  const nr = internalBinding('report');

  // Keep it un-exposed; lest programs play with it
  // leaving us with a lot of unwanted sanity checks.
  let config = {
    events: [],
    signal: 'SIGUSR2',
    filename: '',
    path: '',
    verbose: false
  };
  const report = {
    setDiagnosticReportOptions(options) {
      emitExperimentalWarning('report');
      // Reuse the null and undefined checks. Save
      // space when dealing with large number of arguments.
      const list = parseOptions(options);

      // Flush the stale entries from report, as
      // we are refreshing it, items that the users did not
      // touch may be hanging around stale otherwise.
      config = {};

      // The parseOption method returns an array that include
      // the indices at which valid params are present.
      list.forEach((i) => {
        switch (i) {
          case REPORTEVENTS:
            if (Array.isArray(options.events))
              config.events = options.events;
            else
              throw new ERR_INVALID_ARG_TYPE('events',
                                             'Array',
                                             options.events);
            break;
          case REPORTSIGNAL:
            if (typeof options.signal !== 'string') {
              throw new ERR_INVALID_ARG_TYPE('signal',
                                             'String',
                                             options.signal);
            }
            process.removeListener(config.signal, handleSignal);
            if (config.events.includes('signal'))
              process.on(options.signal, handleSignal);
            config.signal = options.signal;
            break;
          case REPORTFILENAME:
            if (typeof options.filename !== 'string') {
              throw new ERR_INVALID_ARG_TYPE('filename',
                                             'String',
                                             options.filename);
            }
            config.filename = options.filename;
            break;
          case REPORTPATH:
            if (typeof options.path !== 'string')
              throw new ERR_INVALID_ARG_TYPE('path', 'String', options.path);
            config.path = options.path;
            break;
          case REPORTVERBOSE:
            if (typeof options.verbose !== 'string' &&
                typeof options.verbose !== 'boolean') {
              throw new ERR_INVALID_ARG_TYPE('verbose',
                                             'Booelan | String' +
                                             ' (true|false|yes|no)',
                                             options.verbose);
            }
            config.verbose = options.verbose;
            break;
        }
      });
      // Upload this new config to C++ land
      nr.syncConfig(config, true);
    },


    triggerReport(file, err) {
      emitExperimentalWarning('report');
      if (err == null) {
        if (file == null) {
          return nr.triggerReport(new ERR_SYNTHETIC(
            'JavaScript Callstack').stack);
        }
        if (typeof file !== 'string')
          throw new ERR_INVALID_ARG_TYPE('file', 'String', file);
        return nr.triggerReport(file, new ERR_SYNTHETIC(
          'JavaScript Callstack').stack);
      }
      if (typeof err !== 'object')
        throw new ERR_INVALID_ARG_TYPE('err', 'Object', err);
      if (file == null)
        return nr.triggerReport(err.stack);
      if (typeof file !== 'string')
        throw new ERR_INVALID_ARG_TYPE('file', 'String', file);
      return nr.triggerReport(file, err.stack);
    },
    getReport(err) {
      emitExperimentalWarning('report');
      if (err == null) {
        return nr.getReport(new ERR_SYNTHETIC('JavaScript Callstack').stack);
      } else if (typeof err !== 'object') {
        throw new ERR_INVALID_ARG_TYPE('err', 'Objct', err);
      } else {
        return nr.getReport(err.stack);
      }
    }
  };

  // Download the CLI / ENV config into JS land.
  nr.syncConfig(config, false);

  function handleSignal(signo) {
    if (typeof signo !== 'string')
      signo = config.signal;
    nr.onUserSignal(signo);
  }

  if (config.events.includes('signal')) {
    process.on(config.signal, handleSignal);
  }

  function parseOptions(obj) {
    const list = [];
    if (obj == null)
      return list;
    if (obj.events != null)
      list.push(REPORTEVENTS);
    if (obj.signal != null)
      list.push(REPORTSIGNAL);
    if (obj.filename != null)
      list.push(REPORTFILENAME);
    if (obj.path != null)
      list.push(REPORTPATH);
    if (obj.verbose != null)
      list.push(REPORTVERBOSE);
    return list;
  }
  process.report = report;
};
