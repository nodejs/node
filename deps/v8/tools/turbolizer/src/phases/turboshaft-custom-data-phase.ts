// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Phase, PhaseType } from "./phase";

export class TurboshaftCustomDataPhase extends Phase {
  dataTarget: DataTarget;
  data: Array<string>;

  constructor(name: string, dataTarget: DataTarget, dataJSON) {
    super(name, PhaseType.TurboshaftCustomData);
    this.dataTarget = dataTarget;
    this.data = new Array<string>();
    this.parseDataFromJSON(dataJSON);
  }

  private parseDataFromJSON(dataJSON): void {
    if (!dataJSON) return;
    for (const item of dataJSON) {
      this.data[item.key] = item.value;
    }
  }
}

export enum DataTarget {
  Nodes = "operations",
  Blocks = "blocks"
}
