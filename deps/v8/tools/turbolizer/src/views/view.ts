// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GenericPhase } from "../source-resolver";
import { SelectionStorage } from "../selection/selection-storage";

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
  public abstract initializeContent(data: GenericPhase, rememberedSelection: SelectionStorage):
    void;
  public abstract detachSelection(): SelectionStorage;
  public abstract adaptSelection(rememberedSelection: SelectionStorage): SelectionStorage;
  public abstract onresize(): void;
  public abstract searchInputAction(searchInput: HTMLInputElement, e: KeyboardEvent,
                                    onlyVisible: boolean): void;

  constructor(idOrContainer: string | HTMLElement) {
    super(idOrContainer);
  }
}

export enum CodeMode {
  MainSource = "main function",
  InlinedSource = "inlined function"
}
