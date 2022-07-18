// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { GraphView } from "./views/graph-view";
import { ScheduleView } from "./views/schedule-view";
import { SequenceView } from "./views/sequence-view";
import { GenericPhase, SourceResolver } from "./source-resolver";
import { SelectionBroker } from "./selection/selection-broker";
import { PhaseView, View } from "./views/view";
import { GraphPhase } from "./phases/graph-phase/graph-phase";
import { GraphNode } from "./phases/graph-phase/graph-node";
import { storageGetItem, storageSetItem } from "./common/util";
import { PhaseType } from "./phases/phase";
import { TurboshaftGraphView } from "./views/turboshaft-graph-view";

const toolboxHTML = `
<div class="graph-toolbox">
  <select id="phase-select">
    <option disabled selected>(please open a file)</option>
  </select>
  <input id="search-input" type="text" title="search nodes for regex" alt="search node for regex" class="search-input"
    placeholder="find with regexp&hellip;">
  <label><input id="search-only-visible" type="checkbox" name="instruction-address" alt="Apply search to visible nodes only">only visible</label>
</div>`;

export class GraphMultiView extends View {
  sourceResolver: SourceResolver;
  selectionBroker: SelectionBroker;
  graph: GraphView;
  turboshaftGraph: TurboshaftGraphView;
  schedule: ScheduleView;
  sequence: SequenceView;
  selectMenu: HTMLSelectElement;
  currentPhaseView: PhaseView;

  constructor(id, selectionBroker, sourceResolver) {
    super(id);
    const view = this;
    view.sourceResolver = sourceResolver;
    view.selectionBroker = selectionBroker;
    const toolbox = document.createElement("div");
    toolbox.className = "toolbox-anchor";
    toolbox.innerHTML = toolboxHTML;
    view.divNode.appendChild(toolbox);
    const searchInput = toolbox.querySelector("#search-input") as HTMLInputElement;
    const onlyVisibleCheckbox = toolbox.querySelector("#search-only-visible") as HTMLInputElement;
    searchInput.addEventListener("keyup", (e: KeyboardEvent) => {
      view.currentPhaseView?.searchInputAction(searchInput, e, onlyVisibleCheckbox.checked);
    });
    view.divNode.addEventListener("keyup", (e: KeyboardEvent) => {
      if (e.keyCode == 191) { // keyCode == '/'
        searchInput.focus();
      } else if (e.keyCode == 78) { // keyCode == 'n'
        view.displayNextGraphPhase();
      } else if (e.keyCode == 66) { // keyCode == 'b'
        view.displayPreviousGraphPhase();
      }
    });
    searchInput.setAttribute("value", storageGetItem("lastSearch", "", false));
    this.graph = new GraphView(this.divNode, selectionBroker, view.displayPhaseByName.bind(this),
      toolbox.querySelector(".graph-toolbox"));
    this.turboshaftGraph = new TurboshaftGraphView(this.divNode, selectionBroker,
      view.displayPhaseByName.bind(this), toolbox.querySelector(".graph-toolbox"));
    this.schedule = new ScheduleView(this.divNode, selectionBroker);
    this.sequence = new SequenceView(this.divNode, selectionBroker);
    this.selectMenu = toolbox.querySelector("#phase-select") as HTMLSelectElement;
  }

  public createViewElement(): HTMLDivElement {
    const pane = document.createElement("div");
    pane.setAttribute("id", C.MULTIVIEW_ID);
    pane.setAttribute("tabindex", "1");
    pane.className = "viewpane";
    return pane;
  }

  public hide(): void {
    this.container.className = "";
    this.hideCurrentPhase();
    super.hide();
  }

  public show(): void {
    // Insert before is used so that the display is inserted before the
    // resizer for the RangeView.
    this.container.insertBefore(this.divNode, this.container.firstChild);
    this.initializeSelect();
    const lastPhaseIndex = storageGetItem("lastSelectedPhase");
    const initialPhaseIndex = this.sourceResolver.repairPhaseId(lastPhaseIndex);
    this.selectMenu.selectedIndex = initialPhaseIndex;
    this.displayPhase(this.sourceResolver.getPhase(initialPhaseIndex));
  }

  public onresize(): void {
    this.currentPhaseView?.onresize();
  }

  private displayPhase(phase: GenericPhase, selection?: Map<string, GraphNode>): void {
    if (phase.type == PhaseType.Graph) {
      this.displayPhaseView(this.graph, phase, selection);
    } else if (phase.type == PhaseType.TurboshaftGraph) {
      this.displayPhaseView(this.turboshaftGraph, phase, selection);
    } else if (phase.type == PhaseType.Schedule) {
      this.displayPhaseView(this.schedule, phase, selection);
    } else if (phase.type == PhaseType.Sequence) {
      this.displayPhaseView(this.sequence, phase, selection);
    }
  }

  private displayPhaseView(view: PhaseView, data: GenericPhase, selection?: Map<string, GraphNode>):
    void {
    const rememberedSelection = selection ? selection : this.hideCurrentPhase();
    view.initializeContent(data, rememberedSelection);
    this.currentPhaseView = view;
  }

  private displayPhaseByName(phaseName: string, selection?: Map<string, GraphNode>): void {
    const phaseId = this.sourceResolver.getPhaseIdByName(phaseName);
    this.selectMenu.selectedIndex = phaseId;
    this.displayPhase(this.sourceResolver.getPhase(phaseId), selection);
  }

  private displayNextGraphPhase(): void {
    let nextPhaseIndex = this.selectMenu.selectedIndex + 1;
    while (nextPhaseIndex < this.sourceResolver.phases.length) {
      const nextPhase = this.sourceResolver.getPhase(nextPhaseIndex);
      if (nextPhase.isGraph()) {
        this.selectMenu.selectedIndex = nextPhaseIndex;
        storageSetItem("lastSelectedPhase", nextPhaseIndex);
        this.displayPhase(nextPhase);
        break;
      }
      nextPhaseIndex += 1;
    }
  }

  private displayPreviousGraphPhase(): void {
    let previousPhaseIndex = this.selectMenu.selectedIndex - 1;
    while (previousPhaseIndex >= 0) {
      const previousPhase = this.sourceResolver.getPhase(previousPhaseIndex);
      if (previousPhase.isGraph()) {
        this.selectMenu.selectedIndex = previousPhaseIndex;
        storageSetItem("lastSelectedPhase", previousPhaseIndex);
        this.displayPhase(previousPhase);
        break;
      }
      previousPhaseIndex -= 1;
    }
  }

  private initializeSelect(): void {
    const view = this;
    view.selectMenu.innerHTML = "";
    for (const phase of view.sourceResolver.phases) {
      const optionElement = document.createElement("option");
      let maxNodeId = "";
      if (phase instanceof GraphPhase && phase.highestNodeId != 0) {
        maxNodeId = ` ${phase.highestNodeId}`;
      }
      optionElement.text = `${phase.name}${maxNodeId}`;
      view.selectMenu.add(optionElement);
    }
    this.selectMenu.onchange = function (this: HTMLSelectElement) {
      const phaseIndex = this.selectedIndex;
      storageSetItem("lastSelectedPhase", phaseIndex);
      view.displayPhase(view.sourceResolver.getPhase(phaseIndex));
    };
  }

  private hideCurrentPhase(): Map<string, GraphNode> {
    let rememberedSelection = null;
    if (this.currentPhaseView != null) {
      rememberedSelection = this.currentPhaseView.detachSelection();
      this.currentPhaseView.hide();
      this.currentPhaseView = null;
    }
    return rememberedSelection;
  }
}
