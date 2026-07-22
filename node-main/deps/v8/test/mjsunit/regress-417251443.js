// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v0 = `
    const v1 = \`
        v1 >>> v1;
    \`;
    eval(v1);
`;
%RuntimeEvaluateREPL(v0);
