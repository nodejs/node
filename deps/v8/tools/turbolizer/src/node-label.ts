// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { BytecodeOrigin, NodeOrigin } from "./origin";
import { BytecodePosition, SourcePosition } from "./position";

export class NodeLabel {
  id: number;
  label: string;
  title: string;
  live: boolean;
  properties: string;
  sourcePosition: SourcePosition;
  bytecodePosition: BytecodePosition;
  origin: NodeOrigin | BytecodeOrigin;
  opcode: string;
  control: boolean;
  opinfo: string;
  type: string;
  inplaceUpdatePhase: string;

  constructor(id: number, label: string, title: string, live: boolean,
              properties: string, sourcePosition: SourcePosition,
              bytecodePosition: BytecodePosition, origin: NodeOrigin | BytecodeOrigin,
              opcode: string, control: boolean, opinfo: string, type: string) {
    this.id = id;
    this.label = label;
    this.title = title;
    this.live = live;
    this.properties = properties;
    this.sourcePosition = sourcePosition;
    this.bytecodePosition = bytecodePosition;
    this.origin = origin;
    this.opcode = opcode;
    this.control = control;
    this.opinfo = opinfo;
    this.type = type;
    this.inplaceUpdatePhase = null;
  }

  public equals(that?: NodeLabel): boolean {
    if (!that) return false;
    if (this.id !== that.id) return false;
    if (this.label !== that.label) return false;
    if (this.title !== that.title) return false;
    if (this.live !== that.live) return false;
    if (this.properties !== that.properties) return false;
    if (this.opcode !== that.opcode) return false;
    if (this.control !== that.control) return false;
    if (this.opinfo !== that.opinfo) return false;
    return this.type === that.type;
  }

  public getTitle(): string {
    const propsString = this.properties.length == 0 ? "no properties" : `[${this.properties}]`;
    let title = `${this.title}\n${propsString}\n${this.opinfo}`;
    if (this.origin) {
      title += `\nOrigin: ${this.origin.toString()}`;
    }
    if (this.inplaceUpdatePhase) {
      title += `\nInplace update in phase: ${this.inplaceUpdatePhase}`;
    }
    return title;
  }

  public getDisplayLabel(): string {
    const label = `${this.id}: ${this.label}`;
    return label.length > 40 ? `${this.id}: ${this.opcode}` : label;
  }

  public setInplaceUpdatePhase(name: string): void {
    this.inplaceUpdatePhase = name;
  }
}
