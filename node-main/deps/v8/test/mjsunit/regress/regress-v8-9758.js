// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --throws

// Can't put this in a try-catch as that changes the parsing so the crash
// doesn't reproduce.
((a = ((b = a) => {})()) => 1)();
