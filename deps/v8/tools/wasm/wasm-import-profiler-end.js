// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Code to run at shutdown: print out the profiles for all instances.
if (typeof WebAssembly.dumpAllProfiles == "function") WebAssembly.dumpAllProfiles();
