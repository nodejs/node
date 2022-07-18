// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GraphNode } from "../phases/graph-phase/graph-node";

export abstract class View {
  protected container: HTMLElement;
  protected divNode: HTMLElement;
  protected abstract createViewElement(): HTMLElement;

  constructor(idOrContainer: string | HTMLElement) {
    this.container = typeof idOrContainer == "string" ? document.getElementById(idOrContainer) : idOrContainer;
    this.divNode = this.createViewElement();
  }

  public show(): void {
    this.container.appendChild(this.divNode);
  }

  public hide(): void {
    this.container.removeChild(this.divNode);
  }
}

export abstract class PhaseView extends View {
  public abstract initializeContent(data: any, rememberedSelection: Map<string, GraphNode>): void;
  public abstract detachSelection(): Map<string, GraphNode>;
  public abstract onresize(): void;
  public abstract searchInputAction(searchInput: HTMLInputElement, e: Event, onlyVisible: boolean): void;

  constructor(idOrContainer: string | HTMLElement) {
    super(idOrContainer);
  }
}
