// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --print-break-location

// Ensure that --print-break-location correctly collects source positions on
// breaking.
debug.Debug
debugger;
