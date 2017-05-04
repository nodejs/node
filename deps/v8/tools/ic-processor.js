// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function inherits(childCtor, parentCtor) {
  childCtor.prototype.__proto__ = parentCtor.prototype;
};

/**
 * A thin wrapper around shell's 'read' function showing a file name on error.
 */
function readFile(fileName) {
  try {
    return read(fileName);
  } catch (e) {
    print(fileName + ': ' + (e.message || e));
    throw e;
  }
}

/**
 * Parser for dynamic code optimization state.
 */
function parseState(s) {
  switch (s) {
  case "": return Profile.CodeState.COMPILED;
  case "~": return Profile.CodeState.OPTIMIZABLE;
  case "*": return Profile.CodeState.OPTIMIZED;
  }
  throw new Error("unknown code state: " + s);
}


function IcProcessor() {
  var propertyICParser = [parseInt, parseInt, parseInt, null, null, parseInt,
                          null, null, null];
  LogReader.call(this, {
      'code-creation': {
          parsers: [null, parseInt, parseInt, parseInt, parseInt, null, 'var-args'],
          processor: this.processCodeCreation },
      'code-move': { parsers: [parseInt, parseInt],
          processor: this.processCodeMove },
      'code-delete': { parsers: [parseInt],
          processor: this.processCodeDelete },
      'sfi-move': { parsers: [parseInt, parseInt],
          processor: this.processFunctionMove },
      'LoadIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "LoadIC") },
      'StoreIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "StoreIC") },
      'KeyedLoadIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "KeyedLoadIC") },
      'KeyedStoreIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "KeyedStoreIC") },
      'CompareIC': {
        parsers : [parseInt, parseInt, parseInt, parseInt, null, null, null,
                   null, null, null, null],
        processor: this.processCompareIC },
      'BinaryOpIC': {
        parsers : [parseInt, parseInt, parseInt, parseInt, null, null,
                   parseInt],
        processor: this.processBinaryOpIC },
      'ToBooleanIC': {
        parsers : [parseInt, parseInt, parseInt, parseInt, null, null],
        processor: this.processToBooleanIC },
      'PatchIC': {
        parsers : [parseInt, parseInt, parseInt],
        processor: this.processPatchIC },
      });
  this.deserializedEntriesNames_ = [];
  this.profile_ = new Profile();

  this.LoadIC = 0;
  this.StoreIC = 0;
  this.KeyedLoadIC = 0;
  this.KeyedStoreIC = 0;
  this.CompareIC = 0;
  this.BinaryOpIC = 0;
  this.ToBooleanIC = 0;
  this.PatchIC = 0;
}
inherits(IcProcessor, LogReader);

/**
 * @override
 */
IcProcessor.prototype.printError = function(str) {
  print(str);
};

IcProcessor.prototype.processString = function(string) {
  var end = string.length;
  var current = 0;
  var next = 0;
  var line;
  var i = 0;
  var entry;
  while (current < end) {
    next = string.indexOf("\n", current);
    if (next === -1) break;
    i++;
    line = string.substring(current, next);
    current = next + 1;
    this.processLogLine(line);
  }
}

IcProcessor.prototype.processLogFile = function(fileName) {
  this.collectEntries = true
  this.lastLogFileName_ = fileName;
  var line;
  while (line = readline()) {
    this.processLogLine(line);
  }
  print();
  print("=====================");
  print("Load: " + this.LoadIC);
  print("Store: " + this.StoreIC);
  print("KeyedLoad: " + this.KeyedLoadIC);
  print("KeyedStore: " + this.KeyedStoreIC);
  print("CompareIC: " + this.CompareIC);
  print("BinaryOpIC: " + this.BinaryOpIC);
  print("ToBooleanIC: " + this.ToBooleanIC);
  print("PatchIC: " + this.PatchIC);
};

IcProcessor.prototype.addEntry = function(entry) {
  this.entries.push(entry);
}

IcProcessor.prototype.processCodeCreation = function(
    type, kind, timestamp, start, size, name, maybe_func) {
  name = this.deserializedEntriesNames_[start] || name;
  if (name.startsWith("onComplete")) {
    console.log(name);
  }
  if (maybe_func.length) {
    var funcAddr = parseInt(maybe_func[0]);
    var state = parseState(maybe_func[1]);
    this.profile_.addFuncCode(type, name, timestamp, start, size, funcAddr, state);
  } else {
    this.profile_.addCode(type, name, timestamp, start, size);
  }
};


IcProcessor.prototype.processCodeMove = function(from, to) {
  this.profile_.moveCode(from, to);
};


IcProcessor.prototype.processCodeDelete = function(start) {
  this.profile_.deleteCode(start);
};


IcProcessor.prototype.processFunctionMove = function(from, to) {
  this.profile_.moveFunc(from, to);
};

IcProcessor.prototype.formatName = function(entry) {
  if (!entry) return "<unknown>"
  var name = entry.func.getName();
  var re = /(.*):[0-9]+:[0-9]+$/;
  var array = re.exec(name);
  if (!array) return name;
  return entry.getState() + array[1];
}

IcProcessor.prototype.processPropertyIC = function (
    type, pc, line, column, old_state, new_state, map, name, modifier,
    slow_reason) {
  this[type]++;
  var entry = this.profile_.findEntry(pc);
  print(type + " (" + old_state + "->" + new_state + modifier + ") at " +
        this.formatName(entry) + ":" + line + ":" + column + " " + name +
        " (map 0x" + map.toString(16) + ")");
}

IcProcessor.prototype.processCompareIC = function (
    pc, line, column, stub, op, old_left, old_right, old_state, new_left,
    new_right, new_state) {
  var entry = this.profile_.findEntry(pc);
  this.CompareIC++;
  print("CompareIC[" + op + "] ((" +
        old_left + "+" + old_right + "=" + old_state + ")->(" +
        new_left + "+" + new_right + "=" + new_state + ")) at " +
        this.formatName(entry) + ":" + line + ":" + column);
}

IcProcessor.prototype.processBinaryOpIC = function (
    pc, line, column, stub, old_state, new_state, allocation_site) {
  var entry = this.profile_.findEntry(pc);
  this.BinaryOpIC++;
  print("BinaryOpIC (" + old_state + "->" + new_state + ") at " +
        this.formatName(entry) + ":" + line + ":" + column);
}

IcProcessor.prototype.processToBooleanIC = function (
    pc, line, column, stub, old_state, new_state) {
  var entry = this.profile_.findEntry(pc);
  this.ToBooleanIC++;
  print("ToBooleanIC (" + old_state + "->" + new_state + ") at " +
        this.formatName(entry) + ":" + line + ":" + column);
}

IcProcessor.prototype.processPatchIC = function (pc, test, delta) {
  var entry = this.profile_.findEntry(pc);
  this.PatchIC++;
  print("PatchIC (0x" + test.toString(16) + ", " + delta + ") at " +
        this.formatName(entry));
}

function padLeft(s, len) {
  s = s.toString();
  if (s.length < len) {
    var padLength = len - s.length;
    if (!(padLength in padLeft)) {
      padLeft[padLength] = new Array(padLength + 1).join(' ');
    }
    s = padLeft[padLength] + s;
  }
  return s;
};


function ArgumentsProcessor(args) {
  this.args_ = args;
  this.result_ = ArgumentsProcessor.DEFAULTS;

  this.argsDispatch_ = {
    '--range': ['range', 'auto,auto',
        'Specify the range limit as [start],[end]'],
    '--source-map': ['sourceMap', null,
        'Specify the source map that should be used for output']
  };
};


ArgumentsProcessor.DEFAULTS = {
  logFileName: 'v8.log',
  range: 'auto,auto',
};


ArgumentsProcessor.prototype.parse = function() {
  while (this.args_.length) {
    var arg = this.args_.shift();
    if (arg.charAt(0) != '-') {
      this.result_.logFileName = arg;
      continue;
    }
    var userValue = null;
    var eqPos = arg.indexOf('=');
    if (eqPos != -1) {
      userValue = arg.substr(eqPos + 1);
      arg = arg.substr(0, eqPos);
    }
    if (arg in this.argsDispatch_) {
      var dispatch = this.argsDispatch_[arg];
      this.result_[dispatch[0]] = userValue == null ? dispatch[1] : userValue;
    } else {
      return false;
    }
  }
  return true;
};


ArgumentsProcessor.prototype.result = function() {
  return this.result_;
};


ArgumentsProcessor.prototype.printUsageAndExit = function() {

  function padRight(s, len) {
    s = s.toString();
    if (s.length < len) {
      s = s + (new Array(len - s.length + 1).join(' '));
    }
    return s;
  }

  print('Cmdline args: [options] [log-file-name]\n' +
        'Default log file name is "' +
        ArgumentsProcessor.DEFAULTS.logFileName + '".\n');
  print('Options:');
  for (var arg in this.argsDispatch_) {
    var synonyms = [arg];
    var dispatch = this.argsDispatch_[arg];
    for (var synArg in this.argsDispatch_) {
      if (arg !== synArg && dispatch === this.argsDispatch_[synArg]) {
        synonyms.push(synArg);
        delete this.argsDispatch_[synArg];
      }
    }
    print('  ' + padRight(synonyms.join(', '), 20) + " " + dispatch[2]);
  }
  quit(2);
};
