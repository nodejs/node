// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ConsoleTypes_h
#define V8ConsoleTypes_h

namespace blink {

enum MessageLevel {
    DebugMessageLevel = 4,
    LogMessageLevel = 1,
    InfoMessageLevel = 5,
    WarningMessageLevel = 2,
    ErrorMessageLevel = 3
};

}

#endif // !defined(V8ConsoleTypes_h)
