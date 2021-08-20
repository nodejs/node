// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { WebInspector } from "./sourcemap.mjs";
import {
    ParseProcessor, ArgumentsProcessor, readFile,
  } from "./parse-processor.mjs";

function processArguments(args) {
  const processor = new ArgumentsProcessor(args);
  if (processor.parse()) {
    return processor.result();
  } else {
    processor.printUsageAndExit();
  }
}

function initSourceMapSupport() {
  // Pull dev tools source maps  into our name space.
  SourceMap = WebInspector.SourceMap;

  // Overwrite the load function to load scripts synchronously.
  SourceMap.load = function(sourceMapURL) {
    const content = readFile(sourceMapURL);
    const sourceMapObject = (JSON.parse(content));
    return new SourceMap(sourceMapURL, sourceMapObject);
  };
}

const params = processArguments(arguments);
let sourceMap = null;
if (params.sourceMap) {
  initSourceMapSupport();
  sourceMap = SourceMap.load(params.sourceMap);
}
const parseProcessor = new ParseProcessor();
parseProcessor.processLogFile(params.logFileName);
