// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(undefined, Reflect.get(new Error(), 'stack', false));
assertEquals(undefined, Reflect.get(new Error(), 'stack', null));
assertEquals(undefined, Reflect.get(new Error(), 'stack', undefined));
