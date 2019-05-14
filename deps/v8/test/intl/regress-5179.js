// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(String.prototype, Symbol.split, {
    get() {
        return function(obj, limit) {
            return [, null];
        }
    }
});

dtf = new Intl.DateTimeFormat("de", {timeZone:"America/bueNos_airES"});

assertEquals("America/Buenos_Aires", dtf.resolvedOptions().timeZone);
