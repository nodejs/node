// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function formatOrigin(origin) {
  if (origin.nodeId) {
    return `#${origin.nodeId} in phase ${origin.phase}/${origin.reducer}`;
  }
  if (origin.bytecodePosition) {
    return `Bytecode line ${origin.bytecodePosition} in phase ${origin.phase}/${origin.reducer}`;
  }
  return "unknown origin";
}

export class NodeLabel {
  id: number;
  label: string;
  title: string;
  live: boolean;
  properties: string;
  sourcePosition: any;
  origin: any;
  opcode: string;
  control: boolean;
  opinfo: string;
  type: string;
  inplaceUpdatePhase: string;

  constructor(id: number, label: string, title: string, live: boolean, properties: string, sourcePosition: any, origin: any, opcode: string, control: boolean, opinfo: string, type: string) {
    this.id = id;
    this.label = label;
    this.title = title;
    this.live = live;
    this.properties = properties;
    this.sourcePosition = sourcePosition;
    this.origin = origin;
    this.opcode = opcode;
    this.control = control;
    this.opinfo = opinfo;
    this.type = type;
    this.inplaceUpdatePhase = null;
  }

  equals(that?: NodeLabel) {
    if (!that) return false;
    if (this.id != that.id) return false;
    if (this.label != that.label) return false;
    if (this.title != that.title) return false;
    if (this.live != that.live) return false;
    if (this.properties != that.properties) return false;
    if (this.opcode != that.opcode) return false;
    if (this.control != that.control) return false;
    if (this.opinfo != that.opinfo) return false;
    if (this.type != that.type) return false;
    return true;
  }

  getTitle() {
    let propsString = "";
    if (this.properties === "") {
      propsString = "no properties";
    } else {
      propsString = "[" + this.properties + "]";
    }
    let title = this.title + "\n" + propsString + "\n" + this.opinfo;
    if (this.origin) {
      title += `\nOrigin: ${formatOrigin(this.origin)}`;
    }
    if (this.inplaceUpdatePhase) {
      title += `\nInplace update in phase: ${this.inplaceUpdatePhase}`;
    }
    return title;
  }

  getDisplayLabel() {
    const result = `${this.id}: ${this.label}`;
    if (result.length > 40) {
      return `${this.id}: ${this.opcode}`;
    }
    return result;
  }

  setInplaceUpdatePhase(name: string): any {
    this.inplaceUpdatePhase = name;
  }
}
