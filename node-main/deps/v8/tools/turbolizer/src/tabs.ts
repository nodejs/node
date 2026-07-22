// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Tabs {
  private container: HTMLElement;
  private tabBar: HTMLElement;
  private nextTabId: number;

  constructor(container: HTMLElement) {
    this.container = container;
    this.nextTabId = 0;
    this.makeTabBar(container);
  }

  public activateTab(tab: HTMLLIElement): void {
    if (typeof tab.dataset.divid !== "string") return;
    for (const li of this.tabBar.querySelectorAll<HTMLLIElement>("li.active")) {
      li.classList.remove("active");
      this.showTab(li, false);
    }
    tab.classList.add("active");
    this.showTab(tab, true);
  }

  public clearTabsAndContent(): void {
    for (const tab of this.tabBar.querySelectorAll(".nav-tabs > li")) {
      if (!(tab instanceof HTMLLIElement)) continue;
      if (tab.classList.contains("persistent-tab")) continue;
      const tabDiv = document.getElementById(tab.dataset.divid);
      tabDiv.parentNode.removeChild(tabDiv);
      tab.parentNode.removeChild(tab);
    }
  }

  public addTabAndContent(caption: string): [HTMLLIElement, HTMLDivElement] {
    const contentDiv = document.createElement("div");
    contentDiv.className = "tab-content tab-default";
    contentDiv.id = `tab-content-${this.container.id}-${this.nextTabId++}`;
    contentDiv.style.display = "none";
    this.container.appendChild(contentDiv);

    const newTab = this.addTab(caption);
    newTab.dataset.divid = contentDiv.id;
    newTab.draggable = true;
    newTab.ondragstart = this.tabOnDragStart.bind(this);
    const lastTab = this.tabBar.querySelector("li.last-tab");
    this.tabBar.insertBefore(newTab, lastTab);
    return [newTab, contentDiv];
  }

  public addTab(caption: string): HTMLLIElement {
    const newTab = document.createElement("li");
    newTab.innerHTML = caption;
    newTab.id = `tab-header-${this.container.id}-${this.nextTabId++}`;
    const lastTab = this.tabBar.querySelector("li.last-tab");
    this.tabBar.insertBefore(newTab, lastTab);
    return newTab;
  }

  private makeTabBar(container: HTMLElement): void {
    container.classList.add("nav-tabs-container");
    this.tabBar = document.createElement("ul");
    this.tabBar.id = `tab-bar-${container.id}`;
    this.tabBar.className = "nav-tabs";
    this.tabBar.ondrop = this.tabBarOnDrop.bind(this);
    this.tabBar.ondragover = this.tabBarOnDragover.bind(this);
    this.tabBar.onclick = this.tabBarOnClick.bind(this);

    const defaultDiv = document.createElement("div");
    defaultDiv.className = "tab-content tab-default";
    defaultDiv.id = `tab-content-${container.id}-default`;
    container.insertBefore(defaultDiv, container.firstChild);
    container.insertBefore(this.tabBar, container.firstChild);
  }

  private showTab(li: HTMLElement, show: boolean = true) {
    const tabDiv = document.getElementById(li.dataset.divid);
    tabDiv.style.display = show ? "block" : "none";
  }

  private moveTabDiv(tab: HTMLLIElement): void {
    const tabDiv = document.getElementById(tab.dataset.divid);
    tabDiv.style.display = "none";
    tab.classList.remove("active");
    this.tabBar.parentNode.appendChild(tabDiv);
  }

  private tabBarOnDrop(e: DragEvent): void {
    if (!(e.target instanceof HTMLElement)) return;
    e.preventDefault();
    const tabId = e.dataTransfer.getData("text");
    const tab = document.getElementById(tabId) as HTMLLIElement;
    if (tab.parentNode != this.tabBar) {
      this.moveTabDiv(tab);
    }
    const dropTab = e.target.parentNode == this.tabBar
      ? e.target
      : this.tabBar.querySelector("li.last-tab");
    this.tabBar.insertBefore(tab, dropTab);
    this.activateTab(tab);
  }

  private tabBarOnDragover(e: DragEvent): void {
    e.preventDefault();
  }

  private tabOnDragStart(e: DragEvent): void {
    if (!(e.target instanceof HTMLElement)) return;
    e.dataTransfer.setData("text", e.target.id);
  }

  private tabBarOnClick(e: MouseEvent): void {
    const li = e.target as HTMLLIElement;
    this.activateTab(li);
  }
}
