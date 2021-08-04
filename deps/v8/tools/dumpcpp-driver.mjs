// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { WebInspector } from "./sourcemap.mjs";
import {
    CppProcessor, ArgumentsProcessor, UnixCppEntriesProvider,
    WindowsCppEntriesProvider, MacCppEntriesProvider
  } from  "./dumpcpp.mjs";

// Dump C++ symbols of shared library if possible

function processArguments(args) {
  const processor = new ArgumentsProcessor(args);
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
    const content = readFile(sourceMapURL);
    const sourceMapObject = (JSON.parse(content));
    return new SourceMap(sourceMapURL, sourceMapObject);
  };
}

const entriesProviders = {
  'unix': UnixCppEntriesProvider,
  'windows': WindowsCppEntriesProvider,
  'mac': MacCppEntriesProvider
};

const params = processArguments(arguments);
let sourceMap = null;
if (params.sourceMap) {
  initSourceMapSupport();
  sourceMap = SourceMap.load(params.sourceMap);
}

const cppProcessor = new CppProcessor(
  new (entriesProviders[params.platform])(params.nm, params.targetRootFS,
                                          params.apkEmbeddedLibrary),
  params.timedRange, params.pairwiseTimedRange);
cppProcessor.processLogFile(params.logFileName);
cppProcessor.dumpCppSymbols();
