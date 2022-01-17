// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
    CppProcessor, ArgumentsProcessor, LinuxCppEntriesProvider,
    WindowsCppEntriesProvider, MacOSCppEntriesProvider
  } from  "./dumpcpp.mjs";

// Dump C++ symbols of shared library if possible

const entriesProviders = {
  'linux': LinuxCppEntriesProvider,
  'windows': WindowsCppEntriesProvider,
  'macos': MacOSCppEntriesProvider
};

const params = ArgumentsProcessor.process(arguments);
const cppProcessor = new CppProcessor(
  new (entriesProviders[params.platform])(params.nm, params.targetRootFS,
                                          params.apkEmbeddedLibrary),
  params.timedRange, params.pairwiseTimedRange);
await cppProcessor.processLogFile(params.logFileName);
cppProcessor.dumpCppSymbols();
