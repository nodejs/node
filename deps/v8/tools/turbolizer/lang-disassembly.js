// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

PR.registerLangHandler(
    PR.createSimpleLexer(
      [
        [PR.PR_STRING,        /^(?:\'(?:[^\\\'\r\n]|\\.)*(?:\'|$))/, null, '\''],
        [PR.PR_PLAIN,         /^\s+/, null, ' \r\n\t\xA0']
      ],
      [ // fallthroughStylePatterns
        [PR.PR_COMMENT,       /;; debug: position \d+/, null],
      ]),
    ['disassembly']);
