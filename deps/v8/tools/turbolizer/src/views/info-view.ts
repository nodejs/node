// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { View } from "./view";

export class InfoView extends View {
  constructor(idOrContainer: HTMLElement | string) {
    super(idOrContainer);
    fetch("info-view.html")
      .then(response => response.text())
      .then(htmlText => this.divNode.innerHTML = htmlText);
  }

  public createViewElement(): HTMLElement {
    const infoContainer = document.createElement("div");
    infoContainer.classList.add("info-container");
    return infoContainer;
  }
}
