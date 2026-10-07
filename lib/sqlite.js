'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypeSlice,
  ReflectApply,
} = primordials;

const {
  Database,
  Statement,
  Session,
  SQLTagStore,
  constants,
  backup,
  databaseQueryMethods,
  getStatementTraceInfo,
  statementQueryMethods,
  tagStoreQueryMethods,
} = internalBinding('sqlite');
const { tracingChannel } = require('diagnostics_channel');

const queryChannel = tracingChannel('sqlite.query');
// The query methods only ever publish start, end and error, so they check those
// directly. queryChannel.hasSubscribers checks all five events through two
// getter layers, which costs tens of nanoseconds per call before V8 optimizes
// it and slows down every call that throws.
const {
  start: queryStart,
  end: queryEnd,
  error: queryError,
} = queryChannel;

function statementContext(statement, method, parameters) {
  const info = getStatementTraceInfo(statement);
  return {
    __proto__: null,
    sql: info?.[1],
    parameters,
    method,
    database: info?.[0],
    statement,
  };
}

function tagStoreContext(store, method, args) {
  const strings = args[0];
  return {
    __proto__: null,
    // Matches how the tag store builds the SQL it prepares.
    sql: ArrayIsArray(strings) ? ArrayPrototypeJoin(strings, '?') : undefined,
    parameters: ArrayPrototypeSlice(args, 1),
    method,
    database: store.db,
    statement: undefined,
  };
}

const {
  run: statementRun,
  get: statementGet,
  all: statementAll,
} = statementQueryMethods;

Statement.prototype.run = function run() {
  if (queryStart.hasSubscribers || queryEnd.hasSubscribers || queryError.hasSubscribers) {
    const args = ArrayPrototypeSlice(arguments);
    return queryChannel.traceSync(statementRun, statementContext(this, 'run', args), this, ...args);
  }
  return ReflectApply(statementRun, this, arguments);
};

Statement.prototype.get = function get() {
  if (queryStart.hasSubscribers || queryEnd.hasSubscribers || queryError.hasSubscribers) {
    const args = ArrayPrototypeSlice(arguments);
    return queryChannel.traceSync(statementGet, statementContext(this, 'get', args), this, ...args);
  }
  return ReflectApply(statementGet, this, arguments);
};

Statement.prototype.all = function all() {
  if (queryStart.hasSubscribers || queryEnd.hasSubscribers || queryError.hasSubscribers) {
    const args = ArrayPrototypeSlice(arguments);
    return queryChannel.traceSync(statementAll, statementContext(this, 'all', args), this, ...args);
  }
  return ReflectApply(statementAll, this, arguments);
};

const {
  run: tagStoreRun,
  get: tagStoreGet,
  all: tagStoreAll,
} = tagStoreQueryMethods;

SQLTagStore.prototype.run = function run() {
  if (queryStart.hasSubscribers || queryEnd.hasSubscribers || queryError.hasSubscribers) {
    const args = ArrayPrototypeSlice(arguments);
    return queryChannel.traceSync(tagStoreRun, tagStoreContext(this, 'run', args), this, ...args);
  }
  return ReflectApply(tagStoreRun, this, arguments);
};

SQLTagStore.prototype.get = function get() {
  if (queryStart.hasSubscribers || queryEnd.hasSubscribers || queryError.hasSubscribers) {
    const args = ArrayPrototypeSlice(arguments);
    return queryChannel.traceSync(tagStoreGet, tagStoreContext(this, 'get', args), this, ...args);
  }
  return ReflectApply(tagStoreGet, this, arguments);
};

SQLTagStore.prototype.all = function all() {
  if (queryStart.hasSubscribers || queryEnd.hasSubscribers || queryError.hasSubscribers) {
    const args = ArrayPrototypeSlice(arguments);
    return queryChannel.traceSync(tagStoreAll, tagStoreContext(this, 'all', args), this, ...args);
  }
  return ReflectApply(tagStoreAll, this, arguments);
};

const { exec: databaseExec } = databaseQueryMethods;

Database.prototype.exec = function exec() {
  if (queryStart.hasSubscribers || queryEnd.hasSubscribers || queryError.hasSubscribers) {
    const args = ArrayPrototypeSlice(arguments);
    const context = {
      __proto__: null,
      sql: args[0],
      parameters: [],
      method: 'exec',
      database: this,
      statement: undefined,
    };
    return queryChannel.traceSync(databaseExec, context, this, ...args);
  }
  return ReflectApply(databaseExec, this, arguments);
};

module.exports = {
  Database,
  // DEP0210: Documentation-only deprecation, kept for backward
  // compatibility with the class's pre-rename name.
  DatabaseSync: Database,
  Statement,
  // DEP0211: Documentation-only deprecation, kept for backward
  // compatibility with the class's pre-rename name.
  StatementSync: Statement,
  Session,
  constants,
  backup,
};
