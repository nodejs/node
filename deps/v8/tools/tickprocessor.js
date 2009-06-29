// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


function Profile(separateIc) {
  devtools.profiler.Profile.call(this);
  if (!separateIc) {
    this.skipThisFunction = function(name) { return Profile.IC_RE.test(name); };
  }
};
Profile.prototype = devtools.profiler.Profile.prototype;


Profile.IC_RE =
    /^(?:CallIC|LoadIC|StoreIC)|(?:Builtin: (?:Keyed)?(?:Call|Load|Store)IC_)/;


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


function inherits(childCtor, parentCtor) {
  function tempCtor() {};
  tempCtor.prototype = parentCtor.prototype;
  childCtor.prototype = new tempCtor();
};


function TickProcessor(
    cppEntriesProvider, separateIc, ignoreUnknown, stateFilter) {
  devtools.profiler.LogReader.call(this, {
      'shared-library': { parsers: [null, parseInt, parseInt],
          processor: this.processSharedLibrary },
      'code-creation': {
          parsers: [null, this.createAddressParser('code'), parseInt, null],
          processor: this.processCodeCreation, backrefs: true },
      'code-move': { parsers: [this.createAddressParser('code'),
          this.createAddressParser('code-move-to')],
          processor: this.processCodeMove, backrefs: true },
      'code-delete': { parsers: [this.createAddressParser('code')],
          processor: this.processCodeDelete, backrefs: true },
      'tick': { parsers: [this.createAddressParser('code'),
          this.createAddressParser('stack'), parseInt, 'var-args'],
          processor: this.processTick, backrefs: true },
      'profiler': null,
      // Obsolete row types.
      'code-allocate': null,
      'begin-code-region': null,
      'end-code-region': null });

  this.cppEntriesProvider_ = cppEntriesProvider;
  this.ignoreUnknown_ = ignoreUnknown;
  this.stateFilter_ = stateFilter;
  var ticks = this.ticks_ =
    { total: 0, unaccounted: 0, excluded: 0, gc: 0 };

  Profile.prototype.handleUnknownCode = function(
      operation, addr, opt_stackPos) {
    var op = devtools.profiler.Profile.Operation;
    switch (operation) {
      case op.MOVE:
        print('Code move event for unknown code: 0x' + addr.toString(16));
        break;
      case op.DELETE:
        print('Code delete event for unknown code: 0x' + addr.toString(16));
        break;
      case op.TICK:
        // Only unknown PCs (the first frame) are reported as unaccounted,
        // otherwise tick balance will be corrupted (this behavior is compatible
        // with the original tickprocessor.py script.)
        if (opt_stackPos == 0) {
          ticks.unaccounted++;
        }
        break;
    }
  };

  this.profile_ = new Profile(separateIc);
  this.codeTypes_ = {};
  // Count each tick as a time unit.
  this.viewBuilder_ = new devtools.profiler.ViewBuilder(1);
  this.lastLogFileName_ = null;
};
inherits(TickProcessor, devtools.profiler.LogReader);


TickProcessor.VmStates = {
  JS: 0,
  GC: 1,
  COMPILER: 2,
  OTHER: 3,
  EXTERNAL: 4
};


TickProcessor.CodeTypes = {
  CPP: 0,
  SHARED_LIB: 1
};
// Otherwise, this is JS-related code. We are not adding it to
// codeTypes_ map because there can be zillions of them.


TickProcessor.CALL_PROFILE_CUTOFF_PCT = 2.0;


/**
 * @override
 */
TickProcessor.prototype.printError = function(str) {
  print(str);
};


TickProcessor.prototype.setCodeType = function(name, type) {
  this.codeTypes_[name] = TickProcessor.CodeTypes[type];
};


TickProcessor.prototype.isSharedLibrary = function(name) {
  return this.codeTypes_[name] == TickProcessor.CodeTypes.SHARED_LIB;
};


TickProcessor.prototype.isCppCode = function(name) {
  return this.codeTypes_[name] == TickProcessor.CodeTypes.CPP;
};


TickProcessor.prototype.isJsCode = function(name) {
  return !(name in this.codeTypes_);
};


TickProcessor.prototype.processLogFile = function(fileName) {
  this.lastLogFileName_ = fileName;
  var contents = readFile(fileName);
  this.processLogChunk(contents);
};


TickProcessor.prototype.processSharedLibrary = function(
    name, startAddr, endAddr) {
  var entry = this.profile_.addStaticCode(name, startAddr, endAddr);
  this.setCodeType(entry.getName(), 'SHARED_LIB');

  var self = this;
  var libFuncs = this.cppEntriesProvider_.parseVmSymbols(
      name, startAddr, endAddr, function(fName, fStart, fEnd) {
    self.profile_.addStaticCode(fName, fStart, fEnd);
    self.setCodeType(fName, 'CPP');
  });
};


TickProcessor.prototype.processCodeCreation = function(
    type, start, size, name) {
  var entry = this.profile_.addCode(
      this.expandAlias(type), name, start, size);
};


TickProcessor.prototype.processCodeMove = function(from, to) {
  this.profile_.moveCode(from, to);
};


TickProcessor.prototype.processCodeDelete = function(start) {
  this.profile_.deleteCode(start);
};


TickProcessor.prototype.includeTick = function(vmState) {
  return this.stateFilter_ == null || this.stateFilter_ == vmState;
};


TickProcessor.prototype.processTick = function(pc, sp, vmState, stack) {
  this.ticks_.total++;
  if (vmState == TickProcessor.VmStates.GC) this.ticks_.gc++;
  if (!this.includeTick(vmState)) {
    this.ticks_.excluded++;
    return;
  }

  this.profile_.recordTick(this.processStack(pc, stack));
};


TickProcessor.prototype.printStatistics = function() {
  print('Statistical profiling result from ' + this.lastLogFileName_ +
        ', (' + this.ticks_.total +
        ' ticks, ' + this.ticks_.unaccounted + ' unaccounted, ' +
        this.ticks_.excluded + ' excluded).');

  if (this.ticks_.total == 0) return;

  // Print the unknown ticks percentage if they are not ignored.
  if (!this.ignoreUnknown_ && this.ticks_.unaccounted > 0) {
    this.printHeader('Unknown');
    this.printCounter(this.ticks_.unaccounted, this.ticks_.total);
  }

  var flatProfile = this.profile_.getFlatProfile();
  var flatView = this.viewBuilder_.buildView(flatProfile);
  // Sort by self time, desc, then by name, desc.
  flatView.sort(function(rec1, rec2) {
      return rec2.selfTime - rec1.selfTime ||
          (rec2.internalFuncName < rec1.internalFuncName ? -1 : 1); });
  var totalTicks = this.ticks_.total;
  if (this.ignoreUnknown_) {
    totalTicks -= this.ticks_.unaccounted;
  }
  // Our total time contains all the ticks encountered,
  // while profile only knows about the filtered ticks.
  flatView.head.totalTime = totalTicks;

  // Count library ticks
  var flatViewNodes = flatView.head.children;
  var self = this;
  var libraryTicks = 0;
  this.processProfile(flatViewNodes,
      function(name) { return self.isSharedLibrary(name); },
      function(rec) { libraryTicks += rec.selfTime; });
  var nonLibraryTicks = totalTicks - libraryTicks;

  this.printHeader('Shared libraries');
  this.printEntries(flatViewNodes, null,
      function(name) { return self.isSharedLibrary(name); });

  this.printHeader('JavaScript');
  this.printEntries(flatViewNodes, nonLibraryTicks,
      function(name) { return self.isJsCode(name); });

  this.printHeader('C++');
  this.printEntries(flatViewNodes, nonLibraryTicks,
      function(name) { return self.isCppCode(name); });

  this.printHeader('GC');
  this.printCounter(this.ticks_.gc, totalTicks);

  this.printHeavyProfHeader();
  var heavyProfile = this.profile_.getBottomUpProfile();
  var heavyView = this.viewBuilder_.buildView(heavyProfile);
  // To show the same percentages as in the flat profile.
  heavyView.head.totalTime = totalTicks;
  // Sort by total time, desc, then by name, desc.
  heavyView.sort(function(rec1, rec2) {
      return rec2.totalTime - rec1.totalTime ||
          (rec2.internalFuncName < rec1.internalFuncName ? -1 : 1); });
  this.printHeavyProfile(heavyView.head.children);
};


function padLeft(s, len) {
  s = s.toString();
  if (s.length < len) {
    s = (new Array(len - s.length + 1).join(' ')) + s;
  }
  return s;
};


TickProcessor.prototype.printHeader = function(headerTitle) {
  print('\n [' + headerTitle + ']:');
  print('   ticks  total  nonlib   name');
};


TickProcessor.prototype.printHeavyProfHeader = function() {
  print('\n [Bottom up (heavy) profile]:');
  print('  Note: percentage shows a share of a particular caller in the ' +
        'total\n' +
        '  amount of its parent calls.');
  print('  Callers occupying less than ' +
        TickProcessor.CALL_PROFILE_CUTOFF_PCT.toFixed(1) +
        '% are not shown.\n');
  print('   ticks parent  name');
};


TickProcessor.prototype.printCounter = function(ticksCount, totalTicksCount) {
  var pct = ticksCount * 100.0 / totalTicksCount;
  print('  ' + padLeft(ticksCount, 5) + '  ' + padLeft(pct.toFixed(1), 5) + '%');
};


TickProcessor.prototype.processProfile = function(
    profile, filterP, func) {
  for (var i = 0, n = profile.length; i < n; ++i) {
    var rec = profile[i];
    if (!filterP(rec.internalFuncName)) {
      continue;
    }
    func(rec);
  }
};


TickProcessor.prototype.printEntries = function(
    profile, nonLibTicks, filterP) {
  this.processProfile(profile, filterP, function (rec) {
    if (rec.selfTime == 0) return;
    var nonLibPct = nonLibTicks != null ?
        rec.selfTime * 100.0 / nonLibTicks : 0.0;
    print('  ' + padLeft(rec.selfTime, 5) + '  ' +
          padLeft(rec.selfPercent.toFixed(1), 5) + '%  ' +
          padLeft(nonLibPct.toFixed(1), 5) + '%  ' +
          rec.internalFuncName);
  });
};


TickProcessor.prototype.printHeavyProfile = function(profile, opt_indent) {
  var self = this;
  var indent = opt_indent || 0;
  var indentStr = padLeft('', indent);
  this.processProfile(profile, function() { return true; }, function (rec) {
    // Cut off too infrequent callers.
    if (rec.parentTotalPercent < TickProcessor.CALL_PROFILE_CUTOFF_PCT) return;
    print('  ' + padLeft(rec.totalTime, 5) + '  ' +
          padLeft(rec.parentTotalPercent.toFixed(1), 5) + '%  ' +
          indentStr + rec.internalFuncName);
    // Limit backtrace depth.
    if (indent < 10) {
      self.printHeavyProfile(rec.children, indent + 2);
    }
    // Delimit top-level functions.
    if (indent == 0) {
      print('');
    }
  });
};


function CppEntriesProvider() {
};


CppEntriesProvider.prototype.parseVmSymbols = function(
    libName, libStart, libEnd, processorFunc) {
  this.loadSymbols(libName);

  var prevEntry;

  function addPrevEntry(end) {
    // Several functions can be mapped onto the same address. To avoid
    // creating zero-sized entries, skip such duplicates.
    // Also double-check that function belongs to the library address space.
    if (prevEntry && prevEntry.start < end &&
        prevEntry.start >= libStart && end <= libEnd) {
      processorFunc(prevEntry.name, prevEntry.start, end);
    }
  }

  while (true) {
    var funcInfo = this.parseNextLine();
    if (funcInfo === null) {
      continue;
    } else if (funcInfo === false) {
      break;
    }
    if (funcInfo.start < libStart && funcInfo.start < libEnd - libStart) {
      funcInfo.start += libStart;
    }
    addPrevEntry(funcInfo.start);
    prevEntry = funcInfo;
  }
  addPrevEntry(libEnd);
};


CppEntriesProvider.prototype.loadSymbols = function(libName) {
};


CppEntriesProvider.prototype.parseNextLine = function() {
  return false;
};


function UnixCppEntriesProvider(nmExec) {
  this.symbols = [];
  this.parsePos = 0;
  this.nmExec = nmExec;
};
inherits(UnixCppEntriesProvider, CppEntriesProvider);


UnixCppEntriesProvider.FUNC_RE = /^([0-9a-fA-F]{8}) [tTwW] (.*)$/;


UnixCppEntriesProvider.prototype.loadSymbols = function(libName) {
  this.parsePos = 0;
  try {
    this.symbols = [
      os.system(this.nmExec, ['-C', '-n', libName], -1, -1),
      os.system(this.nmExec, ['-C', '-n', '-D', libName], -1, -1)
    ];
  } catch (e) {
    // If the library cannot be found on this system let's not panic.
    this.symbols = ['', ''];
  }
};


UnixCppEntriesProvider.prototype.parseNextLine = function() {
  if (this.symbols.length == 0) {
    return false;
  }
  var lineEndPos = this.symbols[0].indexOf('\n', this.parsePos);
  if (lineEndPos == -1) {
    this.symbols.shift();
    this.parsePos = 0;
    return this.parseNextLine();
  }

  var line = this.symbols[0].substring(this.parsePos, lineEndPos);
  this.parsePos = lineEndPos + 1;
  var fields = line.match(UnixCppEntriesProvider.FUNC_RE);
  return fields ? { name: fields[2], start: parseInt(fields[1], 16) } : null;
};


function WindowsCppEntriesProvider() {
  this.symbols = '';
  this.parsePos = 0;
};
inherits(WindowsCppEntriesProvider, CppEntriesProvider);


WindowsCppEntriesProvider.FILENAME_RE = /^(.*)\.exe$/;


WindowsCppEntriesProvider.FUNC_RE =
    /^ 0001:[0-9a-fA-F]{8}\s+([_\?@$0-9a-zA-Z]+)\s+([0-9a-fA-F]{8}).*$/;


WindowsCppEntriesProvider.prototype.loadSymbols = function(libName) {
  var fileNameFields = libName.match(WindowsCppEntriesProvider.FILENAME_RE);
  // Only try to load symbols for the .exe file.
  if (!fileNameFields) return;
  var mapFileName = fileNameFields[1] + '.map';
  this.symbols = readFile(mapFileName);
};


WindowsCppEntriesProvider.prototype.parseNextLine = function() {
  var lineEndPos = this.symbols.indexOf('\r\n', this.parsePos);
  if (lineEndPos == -1) {
    return false;
  }

  var line = this.symbols.substring(this.parsePos, lineEndPos);
  this.parsePos = lineEndPos + 2;
  var fields = line.match(WindowsCppEntriesProvider.FUNC_RE);
  return fields ?
      { name: this.unmangleName(fields[1]), start: parseInt(fields[2], 16) } :
      null;
};


/**
 * Performs very simple unmangling of C++ names.
 *
 * Does not handle arguments and template arguments. The mangled names have
 * the form:
 *
 *   ?LookupInDescriptor@JSObject@internal@v8@@...arguments info...
 */
WindowsCppEntriesProvider.prototype.unmangleName = function(name) {
  // Empty or non-mangled name.
  if (name.length < 1 || name.charAt(0) != '?') return name;
  var nameEndPos = name.indexOf('@@');
  var components = name.substring(1, nameEndPos).split('@');
  components.reverse();
  return components.join('::');
};


function padRight(s, len) {
  s = s.toString();
  if (s.length < len) {
    s = s + (new Array(len - s.length + 1).join(' '));
  }
  return s;
};


function processArguments(args) {
  var result = {
    logFileName: 'v8.log',
    platform: 'unix',
    stateFilter: null,
    ignoreUnknown: false,
    separateIc: false,
    nm: 'nm'
  };
  var argsDispatch = {
    '-j': ['stateFilter', TickProcessor.VmStates.JS,
        'Show only ticks from JS VM state'],
    '-g': ['stateFilter', TickProcessor.VmStates.GC,
        'Show only ticks from GC VM state'],
    '-c': ['stateFilter', TickProcessor.VmStates.COMPILER,
        'Show only ticks from COMPILER VM state'],
    '-o': ['stateFilter', TickProcessor.VmStates.OTHER,
        'Show only ticks from OTHER VM state'],
    '-e': ['stateFilter', TickProcessor.VmStates.EXTERNAL,
        'Show only ticks from EXTERNAL VM state'],
    '--ignore-unknown': ['ignoreUnknown', true,
        'Exclude ticks of unknown code entries from processing'],
    '--separate-ic': ['separateIc', true,
        'Separate IC entries'],
    '--unix': ['platform', 'unix',
        'Specify that we are running on *nix platform'],
    '--windows': ['platform', 'windows',
        'Specify that we are running on Windows platform'],
    '--nm': ['nm', 'nm',
        'Specify the \'nm\' executable to use (e.g. --nm=/my_dir/nm)']
  };
  argsDispatch['--js'] = argsDispatch['-j'];
  argsDispatch['--gc'] = argsDispatch['-g'];
  argsDispatch['--compiler'] = argsDispatch['-c'];
  argsDispatch['--other'] = argsDispatch['-o'];
  argsDispatch['--external'] = argsDispatch['-e'];

  function printUsageAndExit() {
    print('Cmdline args: [options] [log-file-name]\n' +
          'Default log file name is "v8.log".\n');
    print('Options:');
    for (var arg in argsDispatch) {
      var synonims = [arg];
      var dispatch = argsDispatch[arg];
      for (var synArg in argsDispatch) {
        if (arg !== synArg && dispatch === argsDispatch[synArg]) {
          synonims.push(synArg);
          delete argsDispatch[synArg];
        }
      }
      print('  ' + padRight(synonims.join(', '), 20) + dispatch[2]);
    }
    quit(2);
  }

  while (args.length) {
    var arg = args[0];
    if (arg.charAt(0) != '-') {
      break;
    }
    args.shift();
    var userValue = null;
    var eqPos = arg.indexOf('=');
    if (eqPos != -1) {
      userValue = arg.substr(eqPos + 1);
      arg = arg.substr(0, eqPos);
    }
    if (arg in argsDispatch) {
      var dispatch = argsDispatch[arg];
      result[dispatch[0]] = userValue == null ? dispatch[1] : userValue;
    } else {
      printUsageAndExit();
    }
  }

  if (args.length >= 1) {
      result.logFileName = args.shift();
  }
  return result;
};


var params = processArguments(arguments);
var tickProcessor = new TickProcessor(
    params.platform == 'unix' ? new UnixCppEntriesProvider(params.nm) :
        new WindowsCppEntriesProvider(),
    params.separateIc,
    params.ignoreUnknown,
    params.stateFilter);
tickProcessor.processLogFile(params.logFileName);
tickProcessor.printStatistics();
