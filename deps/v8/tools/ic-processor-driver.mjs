// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Processor } from "./system-analyzer/processor.mjs";
import { BaseArgumentsProcessor} from "./arguments.mjs";

class ArgumentsProcessor extends BaseArgumentsProcessor {
  getArgsDispatch() {
    return {
      '--range': ['range', 'auto,auto',
          'Specify the range limit as [start],[end]'],
    };
  }
  getDefaultResults() {
   return {
      logFileName: 'v8.log',
      range: 'auto,auto',
    };
  }
}

const params = ArgumentsProcessor.process(arguments);
const processor = new Processor();
await processor.processLogFile(params.logFileName);

const typeAccumulator = new Map();

const accumulator = {
  __proto__: null,
  LoadGlobalIC: 0,
  StoreGlobalIC: 0,
  LoadIC: 0,
  StoreIC: 0,
  KeyedLoadIC: 0,
  KeyedStoreIC: 0,
  StoreInArrayLiteralIC: 0,
}
for (const ic of processor.icTimeline.all) {
  console.log(Object.values(ic));
  accumulator[ic.type]++;
}

console.log("========================================");
for (const key of Object.keys(accumulator)) {
  console.log(key + ": " + accumulator[key]);
}
