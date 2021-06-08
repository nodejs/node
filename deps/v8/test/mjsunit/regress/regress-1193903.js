// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var no_sync_uninternalized = "no " + "sync";
%InternalizeString(no_sync_uninternalized);

// Make sure %GetOptimizationStatus works with a non-internalized string
// parameter.
%GetOptimizationStatus(function() {}, no_sync_uninternalized)
