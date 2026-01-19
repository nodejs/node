// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Access any property that's also available on the global of the other realm.
this.__defineGetter__("Object", ()=>0);
__proto__ = Realm.global(Realm.create());
Object;
