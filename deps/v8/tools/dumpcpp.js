// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dump C++ symbols of shared library if possible

function processArguments(args) {
  var processor = new ArgumentsProcessor(args);
  if (processor.parse()) {
    return processor.result();
  } else {
    processor.printUsageAndExit();
  }
}

function initSourceMapSupport() {
  // Pull dev tools source maps into our name space.
  SourceMap = WebInspector.SourceMap;

  // Overwrite the load function to load scripts synchronously.
  SourceMap.load = function(sourceMapURL) {
    var content = readFile(sourceMapURL);
    var sourceMapObject = (JSON.parse(content));
    return new SourceMap(sourceMapURL, sourceMapObject);
  };
}

var entriesProviders = {
  'unix': UnixCppEntriesProvider,
  'windows': WindowsCppEntriesProvider,
  'mac': MacCppEntriesProvider
};

var params = processArguments(arguments);
var sourceMap = null;
if (params.sourceMap) {
  initSourceMapSupport();
  sourceMap = SourceMap.load(params.sourceMap);
}

function CppProcessor(cppEntriesProvider, timedRange, pairwiseTimedRange) {
  LogReader.call(this, {
      'shared-library': { parsers: [null, parseInt, parseInt],
          processor: this.processSharedLibrary }
  }, timedRange, pairwiseTimedRange);

  this.cppEntriesProvider_ = cppEntriesProvider;
  this.codeMap_ = new CodeMap();
  this.lastLogFileName_ = null;
}
inherits(CppProcessor, LogReader);

/**
 * @override
 */
CppProcessor.prototype.printError = function(str) {
  print(str);
};

CppProcessor.prototype.processLogFile = function(fileName) {
  this.lastLogFileName_ = fileName;
  var line;
  while (line = readline()) {
    this.processLogLine(line);
  }
};

CppProcessor.prototype.processLogFileInTest = function(fileName) {
   // Hack file name to avoid dealing with platform specifics.
  this.lastLogFileName_ = 'v8.log';
  var contents = readFile(fileName);
  this.processLogChunk(contents);
};

CppProcessor.prototype.processSharedLibrary = function(
    name, startAddr, endAddr) {
  var self = this;
  var libFuncs = this.cppEntriesProvider_.parseVmSymbols(
      name, startAddr, endAddr, function(fName, fStart, fEnd) {
    var entry = new CodeMap.CodeEntry(fEnd - fStart, fName, 'CPP');
    self.codeMap_.addStaticCode(fStart, entry);
  });
};

CppProcessor.prototype.dumpCppSymbols = function() {
  var staticEntries = this.codeMap_.getAllStaticEntriesWithAddresses();
  var total = staticEntries.length;
  for (var i = 0; i < total; ++i) {
    var entry = staticEntries[i];
    var printValues = ['cpp', '0x' + entry[0].toString(16), entry[1].size,
                       '"' + entry[1].name + '"'];
    print(printValues.join(','));
  }
};

var cppProcessor = new CppProcessor(
  new (entriesProviders[params.platform])(params.nm, params.targetRootFS),
  params.timedRange, params.pairwiseTimedRange);
cppProcessor.processLogFile(params.logFileName);
cppProcessor.dumpCppSymbols();
