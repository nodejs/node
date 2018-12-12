
export class Tabs {
    container: HTMLElement
    tabBar: HTMLElement;
    nextTabId: number;

    mkTabBar(container: HTMLElement) {
        container.classList.add("nav-tabs-container")
        this.tabBar = document.createElement("ul");
        this.tabBar.id = `tab-bar-${container.id}`;
        this.tabBar.className = "nav-tabs";
        this.tabBar.ondrop = this.tabBarOnDrop.bind(this);
        this.tabBar.ondragover = this.tabBarOnDragover.bind(this);
        this.tabBar.onclick = this.tabBarOnClick.bind(this);

        const defaultDiv = document.createElement("div");
        defaultDiv.className = "tab-content tab-default";
        defaultDiv.id = `tab-content-${container.id}-default`;
        container.insertBefore(defaultDiv, container.firstChild)
        container.insertBefore(this.tabBar, container.firstChild);
    }

    constructor(container: HTMLElement) {
        this.container = container;
        this.nextTabId = 0;
        this.mkTabBar(container);
    }

    showTab(li: HTMLElement, show: boolean = true) {
        const tabDiv = document.getElementById(li.dataset.divid);
        tabDiv.style.display = show ? "block" : "none";
    }

    activateTab(tab: HTMLLIElement) {
        if (typeof tab.dataset.divid !== "string") return;
        for (const li of this.tabBar.querySelectorAll<HTMLLIElement>("li.active")) {
            li.classList.remove("active");
            this.showTab(li, false);
        }
        tab.classList.add("active");
        this.showTab(tab, true);
    }

    addTab(caption: string): HTMLLIElement {
        const newTab = document.createElement("li");
        newTab.innerHTML = caption;
        newTab.id = `tab-header-${this.container.id}-${this.nextTabId++}`;
        const lastTab = this.tabBar.querySelector("li.open-tab");
        this.tabBar.insertBefore(newTab, lastTab);
        return newTab;
    }

    addTabAndContent(caption: string): [HTMLLIElement, HTMLDivElement] {
        const contentDiv = document.createElement("div");
        contentDiv.className = "tab-content tab-default";
        contentDiv.id = `tab-content-${this.container.id}-${this.nextTabId++}`;
        this.container.appendChild(contentDiv);

        const newTab = this.addTab(caption);
        newTab.dataset.divid = contentDiv.id;
        newTab.draggable = true;
        newTab.ondragstart = this.tabOnDragStart.bind(this);
        const lastTab = this.tabBar.querySelector("li.open-tab");
        this.tabBar.insertBefore(newTab, lastTab);
        return [newTab, contentDiv];
    }

    moveTabDiv(tab: HTMLLIElement) {
        const tabDiv = document.getElementById(tab.dataset.divid);
        tabDiv.style.display = "none";
        tab.classList.remove("active");
        this.tabBar.parentNode.appendChild(tabDiv);
    }

    tabBarOnDrop(e) {
        e.preventDefault();
        const tabId = e.dataTransfer.getData("text");
        const tab = document.getElementById(tabId) as HTMLLIElement;
        if (tab.parentNode != this.tabBar) {
            this.moveTabDiv(tab);
        }
        const dropTab =
            e.target.parentNode == this.tabBar
                ? e.target : this.tabBar.querySelector("li.open-tab");
        this.tabBar.insertBefore(tab, dropTab);
        this.activateTab(tab);
    }

    tabBarOnDragover(e) {
        e.preventDefault();
    }

    tabOnDragStart(e) {
        e.dataTransfer.setData("text", e.target.id);
    }

    tabBarOnClick(e: MouseEvent) {
        const li = e.target as HTMLLIElement;
        this.activateTab(li);
    }
}
