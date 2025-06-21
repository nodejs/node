// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function set_value(obj, prop) { return obj[prop] = undefined }

var object1 = new Proxy({}, {set: true});
try { set_value(object1, "bla"); } catch (_) {};
try { set_value(object1, "0"); } catch (_) {};
try { set_value(object1, Symbol()); } catch(_) {};

set_value({}, 1073741824)

var target = {};
var handler = { set() { return 42 } };
var object2 = new Proxy(target, handler);
Object.defineProperty(target, 'bla', { value: 0 });
set_value(object2, 'bla', 0);
