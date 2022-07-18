// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { SourcePosition } from "./position";

export class Source {
  sourceName: string;
  functionName: string;
  sourceText: string;
  sourceId: number;
  backwardsCompatibility: boolean;
  sourcePositions: Array<SourcePosition>;
  startPosition?: number;
  endPosition?: number;

  constructor(sourceName: string, functionName: string, sourceText: string, sourceId: number,
              backwardsCompatibility: boolean, sourcePositions?: Array<SourcePosition>,
              startPosition?: number, endPosition?: number) {
    this.sourceName = sourceName;
    this.functionName = functionName;
    this.sourceText = sourceText;
    this.sourceId = sourceId;
    this.backwardsCompatibility = backwardsCompatibility;
    this.sourcePositions = sourcePositions ?? new Array<SourcePosition>();
    this.startPosition = startPosition;
    this.endPosition = endPosition;
  }

  public toString(): string {
    return `${this.sourceName}:${this.functionName}`;
  }
}
