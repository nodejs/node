// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ill-formed and valid calendar should throw RangeError.
assertThrows(
    'new Intl.DateTimeFormat("en", {calendar: "gregorian"})',
    RangeError);
