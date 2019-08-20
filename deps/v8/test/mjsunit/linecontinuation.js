// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://tc39.github.io/ecma262/#sec-literals-string-literals
//
// LineContinuation ::
//   \ LineTerminatorSequence
//
// LineTerminatorSequence ::
//   <LF>
//   <CR>[lookahead ≠ <LF>]
//   <LS>
//   <PS>
//   <CR><LF>

// LineTerminatorSequence :: <LF>
assertEquals('', eval('"\\\n"'));
assertEquals('', eval("'\\\n'"));
assertEquals('', eval('`\\\n`'));

// LineTerminatorSequence :: <CR>[lookahead ≠ <LF>]
assertEquals('', eval('"\\\r"'));
assertEquals('', eval("'\\\r'"));
assertEquals('', eval('`\\\r`'));

// LineTerminatorSequence :: <LS>
assertEquals('', eval('"\\\u2028"'));
assertEquals('', eval("'\\\u2028'"));
assertEquals('', eval('`\\\u2028`'));

// LineTerminatorSequence :: <PS>
assertEquals('', eval('"\\\u2029"'));
assertEquals('', eval("'\\\u2029'"));
assertEquals('', eval('`\\\u2029`'));

// LineTerminatorSequence :: <CR><LF>
assertEquals('', eval('"\\\r\n"'));
assertEquals('', eval("'\\\r\n'"));
assertEquals('', eval('`\\\r\n`'));
