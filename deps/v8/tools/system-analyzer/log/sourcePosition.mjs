// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { Event } from './log.mjs';

class SourcePositionLogEvent extends Event {
  constructor(type, time, file, line, col, script) {
    super(type, time);
    this.file = file;
    this.line = line;
    this.col = col;
    this.script = script;
  }
}

export { SourcePositionLogEvent };
