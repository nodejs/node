// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  public abstract initializeContent(data: any, rememberedSelection: Set<any>): void;
  public abstract detachSelection(): Set<string>;
  public abstract onresize(): void;
  public abstract searchInputAction(searchInput: HTMLInputElement, e: Event, onlyVisible: boolean): void;

  constructor(idOrContainer: string | HTMLElement) {
    super(idOrContainer);
  }

  public isScrollable(): boolean {
    return false;
  }
}
