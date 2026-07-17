// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --sandbox-testing --expose-gc

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let fmt;
let addr;

const evil = new Proxy({}, {
  get: (t, p) => (p === Symbol.toPrimitive || p === 'valueOf') ? () => {
    if (!fired) { fired = true; mem.setUint32(addr + 16, 0x1, true); gc(); }
    return 42;
  } : undefined
});

function reset() {
  fired = false;
  fmt = new Intl.NumberFormat("en-US");
  addr = Sandbox.getAddressOf(fmt);
}

reset();
fmt.format(evil);

reset();
fmt.formatToParts(evil);

reset();
fmt.formatRange(-100, evil);

reset();
fmt.formatRange(evil, 10000);

reset();
fmt.formatRangeToParts(-100, evil);

reset();
fmt.formatRangeToParts(evil, 10000);
