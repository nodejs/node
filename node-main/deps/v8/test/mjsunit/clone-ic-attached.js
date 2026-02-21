// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation


function f(x) {return {...x,b:2}};
let o={a:1}

// Ensure the clone ic does not generate detached maps.

assertTrue(%HaveSameMap(f(o), f(o)));
assertTrue(%HaveSameMap(f(o), f(o)));
assertTrue(%HaveSameMap(f(o), f(o)));
assertTrue(%HaveSameMap(f(o), f(o)));

assertTrue(%HaveSameMap({...o},{...o}));
assertTrue(%HaveSameMap({...o},{...o}));
assertTrue(%HaveSameMap({...o},{...o}));
assertTrue(%HaveSameMap({...o},{...o}));
assertTrue(%HaveSameMap({...o},{...o}));
