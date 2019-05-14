import { View } from "./view";

export class InfoView extends View {

  constructor(idOrContainer: HTMLElement | string) {
    super(idOrContainer);
    fetch("info-view.html")
      .then(response => response.text())
      .then(htmlText => this.divNode.innerHTML = htmlText);
  }

  createViewElement(): HTMLElement {
    const infoContainer = document.createElement("div");
    infoContainer.classList.add("info-container");
    return infoContainer;
  }
}
