// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --expose-externalize-string

const worker = new Worker('onmessage = function() {};', {type: 'string'});

assertThrows(() => new worker.terminate(), TypeError);
assertThrows(() => new worker.terminateAndWait(), TypeError);
assertThrows(() => new worker.postMessage(''), TypeError);
assertThrows(() => new worker.getMessage(), TypeError);

worker.terminateAndWait();

assertThrows(() => new gc(), TypeError);

assertThrows(() => new externalizeString(''), TypeError);
assertThrows(() => new createExternalizableString(''), TypeError);
assertThrows(() => new createExternalizableTwoByteString(''), TypeError);
assertThrows(() => new isOneByteString(''), TypeError);
