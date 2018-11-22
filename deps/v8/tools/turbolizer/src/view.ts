// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export abstract class View {
  container: HTMLElement;
  divNode: HTMLElement;
  abstract initializeContent(data: any, rememberedSelection: Selection): void;
  abstract createViewElement(): HTMLElement;
  abstract deleteContent(): void;
  abstract detachSelection(): Set<string>;

  constructor(idOrContainer: string | HTMLElement) {
    this.container = typeof idOrContainer == "string" ? document.getElementById(idOrContainer) : idOrContainer;
    this.divNode = this.createViewElement();
  }

  isScrollable(): boolean {
    return false;
  }

  show(data, rememberedSelection): void {
    this.initializeContent(data, rememberedSelection);
    this.container.appendChild(this.divNode);
  }

  hide(): void {
    this.container.removeChild(this.divNode);
    this.deleteContent();
  }
}

export interface PhaseView {
  onresize();
  searchInputAction(searchInput: HTMLInputElement, e: Event);
}
