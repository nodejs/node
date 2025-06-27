// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/regress/regress-crbug-1321899.js');

const realm = Realm.create();
const globalProxy = Realm.global(realm);

checkNoAccess(globalProxy, /no access/);
