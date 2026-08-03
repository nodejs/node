// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../common/constants";
import * as d3 from "d3";
import { partial, storageSetItem } from "../common/util";
import { SelectionMap } from "../selection/selection-map";
import { Graph } from "../graph";
import { SelectionBroker } from "../selection/selection-broker";
import { GraphNode } from "../phases/graph-phase/graph-node";
import { GraphEdge } from "../phases/graph-phase/graph-edge";
import { GraphLayout } from "../graph-layout";
import { GraphPhase, GraphStateType } from "../phases/graph-phase/graph-phase";
import { NodeOrigin } from "../origin";
import { MovableView } from "./movable-view";
import { ClearableHandler, NodeSelectionHandler } from "../selection/selection-handler";
import { GenericPosition } from "../source-resolver";
import { OutputVisibilityType } from "../node";
import { SelectionStorage } from "../selection/selection-storage";

export class GraphView extends MovableView<Graph> {
  graphLayout: GraphLayout;
  visibleNodes: d3.Selection<any, GraphNode, any, any>;
  visibleEdges: d3.Selection<any, GraphEdge, any, any>;
  visibleBubbles: d3.Selection<any, any, any, any>;
  drag: d3.DragBehavior<any, GraphNode, GraphNode>;

  constructor(idOrContainer: string | HTMLElement, broker: SelectionBroker,
              showPhaseByName: (name: string, selection: SelectionStorage) => void,
              toolbox: HTMLElement) {
    super(idOrContainer, broker, showPhaseByName, toolbox);

    this.state.selection = new SelectionMap(node => node.identifier(), node => {
      if (node instanceof GraphNode) return node.nodeLabel?.origin?.identifier();
      return node?.origin?.identifier();
    });

    this.nodeSelectionHandler = this.initializeNodeSelectionHandler();
    this.svg.on("click", () => this.nodeSelectionHandler.clear());

    this.visibleEdges = this.graphElement.append("g");
    this.visibleNodes = this.graphElement.append("g");

    this.drag = d3.drag<any, GraphNode, GraphNode>()
      .on("drag",  (node: GraphNode) => {
        node.x += d3.event.dx;
        node.y += d3.event.dy;
        this.updateGraphVisibility();
      });
  }

  public initializeContent(data: GraphPhase, rememberedSelection: SelectionStorage): void {
    this.show();
    this.addImgInput("layout", "layout graph",
      partial(this.layoutAction, this));
    this.addImgInput("show-all", "show all nodes",
      partial(this.showAllAction, this));
    this.addImgInput("show-control", "show only control nodes",
      partial(this.showControlAction, this));
    this.addImgInput("hide-unselected", "hide unselected",
      partial(this.hideUnselectedAction, this));
    this.addImgInput("hide-selected", "hide selected",
      partial(this.hideSelectedAction, this));
    this.addImgInput("zoom-selection", "zoom selection",
      partial(this.zoomSelectionAction, this));
    this.addToggleImgInput("toggle-hide-dead", "toggle hide dead nodes",
      this.state.hideDead, partial(this.toggleHideDeadAction, this));
    this.addToggleImgInput("toggle-types", "toggle types",
      this.state.showTypes, partial(this.toggleTypesAction, this));
    this.addToggleImgInput("toggle-cache-layout", "toggle saving graph layout",
      this.state.cacheLayout, partial(this.toggleLayoutCachingAction, this));

    this.phaseName = data.name;
    const adaptedSelection = this.createGraph(data, rememberedSelection);
    this.broker.addNodeHandler(this.nodeSelectionHandler);

    const selectedNodes = adaptedSelection.isAdapted()
      ? this.attachSelection(adaptedSelection.adaptedNodes)
      : null;

    if (selectedNodes?.length > 0) {
      this.connectVisibleSelectedElements(this.state.selection);
      this.updateGraphVisibility();
      this.viewSelection();
    } else {
      if (this.state.cacheLayout && data.transform) {
        this.viewTransformMatrix(data.transform);
      } else {
        this.viewWholeGraph();
      }
    }
  }

  public updateGraphVisibility(): void {
    const view = this;
    const graph = this.graph;
    const state = this.state;
    if (!graph) return;

    const filteredEdges = [
      ...graph.filteredEdges(edge => graph.isRendered()
        && edge.source.visible && edge.target.visible)
    ];

    const selEdges = view.visibleEdges
      .selectAll<SVGPathElement, GraphEdge>("path")
      .data(filteredEdges, edge => edge.toString());

    // remove old links
    selEdges.exit().remove();

    // add new paths
    const newEdges = selEdges
      .enter()
      .append("path")
      .style("marker-end", "url(#end-arrow)")
      .attr("id", edge => `e,${edge.toString()}`)
      .on("click",  edge => {
        d3.event.stopPropagation();
        if (!d3.event.shiftKey) {
          view.nodeSelectionHandler.clear();
        }
        view.nodeSelectionHandler.select([edge.source, edge.target], true, false);
      })
      .attr("adjacentToHover", "false")
      .classed("value", edge => edge.type === "value" || edge.type === "context")
      .classed("control", edge => edge.type === "control")
      .classed("effect", edge => edge.type === "effect")
      .classed("frame-state", edge => edge.type === "frame-state")
      .attr("stroke-dasharray", edge => {
        if (edge.type === "frame-state") return "10,10";
        return edge.type === "effect" ? "5,5" : "";
      });

    const newAndOldEdges = newEdges.merge(selEdges);

    newAndOldEdges.classed("hidden", edge => !edge.isVisible());

    // select existing nodes
    const filteredNodes = [...graph.nodes(node => graph.isRendered() && node.visible)];
    const allNodes = view.visibleNodes.selectAll<SVGGElement, GraphNode>("g");
    const selNodes = allNodes.data(filteredNodes, node => node.toString());

    // remove old nodes
    selNodes.exit().remove();

    // add new nodes
    const newGs = selNodes.enter()
      .append("g")
      .classed("turbonode", true)
      .classed("control", node => node.isControl())
      .classed("live", node => node.isLive())
      .classed("dead", node => !node.isLive())
      .classed("javascript", node => node.isJavaScript())
      .classed("input", node => node.isInput())
      .classed("simplified", node => node.isSimplified())
      .classed("machine", node => node.isMachine())
      .on("mouseenter", node => {
        const visibleEdges = view.visibleEdges.selectAll<SVGPathElement, GraphEdge>("path");
        const adjInputEdges = visibleEdges.filter(edge => edge.target === node);
        const adjOutputEdges = visibleEdges.filter(edge => edge.source === node);
        adjInputEdges.attr("relToHover", "input");
        adjOutputEdges.attr("relToHover", "output");
        const visibleNodes = view.visibleNodes.selectAll<SVGGElement, GraphNode>("g");
        const adjInputNodes = adjInputEdges.data().map(edge => edge.source);
        visibleNodes.data<GraphNode>(adjInputNodes, node => node.toString())
          .attr("relToHover", "input");
        const adjOutputNodes = adjOutputEdges.data().map(edge => edge.target);
        visibleNodes.data<GraphNode>(adjOutputNodes, node => node.toString())
          .attr("relToHover", "output");
        view.hoveredNodeIdentifier = node.identifier();
        view.updateGraphVisibility();
      })
      .on("mouseleave", node => {
        const visibleEdges = view.visibleEdges.selectAll<SVGPathElement, GraphEdge>("path");
        const adjEdges = visibleEdges.filter(edge => edge.target === node || edge.source === node);
        adjEdges.attr("relToHover", "none");
        const adjNodes = adjEdges.data().map(edge => edge.target)
          .concat(adjEdges.data().map(edge => edge.source));
        const visibleNodes = view.visibleNodes.selectAll<SVGPathElement, GraphNode>("g");
        visibleNodes.data(adjNodes, node => node.toString()).attr("relToHover", "none");
        view.hoveredNodeIdentifier = null;
        view.updateGraphVisibility();
      })
      .on("click", node => {
        if (!d3.event.shiftKey) view.nodeSelectionHandler.clear();
        view.nodeSelectionHandler.select([node], undefined, false);
        d3.event.stopPropagation();
      })
      .call(view.drag);

    newGs.each(function (node: GraphNode) {
      const svg = d3.select<SVGGElement, GraphNode>(this);
      svg.append("rect")
        .attr("rx", 10)
        .attr("ry", 10)
        .attr("width", node => node.getWidth())
        .attr("height", node => node.getHeight(view.state.showTypes));

      svg.append("text")
        .classed("label", true)
        .attr("text-anchor", "right")
        .attr("dx", 5)
        .attr("dy", 5)
        .append("tspan")
        .text(node.getDisplayLabel())
        .append("title")
        .text(node.getTitle());

      if (node.nodeLabel.type) {
        svg.append("text")
          .classed("label", true)
          .classed("type", true)
          .attr("text-anchor", "right")
          .attr("dx", 5)
          .attr("dy", node.labelBox.height + 5)
          .append("tspan")
          .text(node.getDisplayType())
          .append("title")
          .text(node.getType());
      }
      view.appendInputAndOutputBubbles(svg, node);
    });

    const newAndOldNodes = newGs.merge(selNodes);

    newAndOldNodes.select<SVGTextElement>(".type").each(function () {
      this.setAttribute("visibility", view.state.showTypes ? "visible" : "hidden");
    });

    newAndOldNodes
      .classed("selected", node => state.selection.isSelected(node))
      .attr("transform", node => `translate(${node.x},${node.y})`)
      .select("rect")
      .attr("height", node => node.getHeight(view.state.showTypes));

    view.visibleBubbles = view.svg.selectAll("circle");

    view.updateInputAndOutputBubbles();

    graph.maxGraphX = graph.maxGraphNodeX;
    newAndOldEdges.attr("d", edge => edge.generatePath(graph, view.state.showTypes));
  }

  public svgKeyDown(): void {
    let eventHandled = true; // unless the below switch defaults
    switch (d3.event.keyCode) {
      case 38: // UP
      case 40: // DOWN
        this.showSelectionFrontierNodes(d3.event.keyCode == 38, undefined, true);
        break;
      case 49:
      case 50:
      case 51:
      case 52:
      case 53:
      case 54:
      case 55:
      case 56:
      case 57: // '1'-'9'
        this.showSelectionFrontierNodes(true,
          (edge: GraphEdge, index: number) => index == (d3.event.keyCode - 49),
          !d3.event.ctrlKey);
        break;
      case 65: // 'a'
        this.selectAllNodes();
        break;
      case 67: // 'c'
        this.showSelectionFrontierNodes(d3.event.altKey,
          (edge: GraphEdge) => edge.type === "control",
          true);
        break;
      case 69: // 'e'
        this.showSelectionFrontierNodes(d3.event.altKey,
          (edge: GraphEdge) => edge.type === "effect",
          true);
        break;
      case 72: // 'h'
        this.showHoveredNodeHistory();
        break;
      case 73: // 'i'
        if (!d3.event.ctrlKey && !d3.event.shiftKey) {
          this.showSelectionFrontierNodes(true, undefined, false);
        } else {
          eventHandled = false;
        }
        break;
      case 79: // 'o'
        this.showSelectionFrontierNodes(false, undefined, false);
        break;
      case 80: // 'p'
        this.selectOrigins();
        break;
      case 82: // 'r'
        if (!d3.event.ctrlKey && !d3.event.shiftKey) {
          this.layoutAction(this);
        } else {
          eventHandled = false;
        }
        break;
      case 83: // 's'
        if (!d3.event.ctrlKey && !d3.event.shiftKey) {
          this.hideSelectedAction(this);
        } else {
          eventHandled = false;
        }
        break;
      case 85: // 'u'
        if (!d3.event.ctrlKey && !d3.event.shiftKey) {
          this.hideUnselectedAction(this);
        } else {
          eventHandled = false;
        }
        break;
      case 97:
      case 98:
      case 99:
      case 100:
      case 101:
      case 102:
      case 103:
      case 104:
      case 105: // 'numpad 1'-'numpad 9'
        this.showSelectionFrontierNodes(true,
          (edge: GraphEdge, index) => index == (d3.event.keyCode - 97),
          !d3.event.ctrlKey);
        break;
      default:
        eventHandled = false;
        break;
    }
    if (eventHandled) d3.event.preventDefault();
  }

  public searchInputAction(searchInput: HTMLInputElement, e: KeyboardEvent, onlyVisible: boolean):
    void {
    if (e.keyCode == 13) {
      this.nodeSelectionHandler.clear();
      const query = searchInput.value;
      storageSetItem("lastSearch", query);
      if (query.length == 0) return;

      const reg = new RegExp(query);
      const filterFunction = (node: GraphNode) => {
        return (reg.exec(node.getDisplayLabel()) !== null ||
          (this.state.showTypes && reg.exec(node.getDisplayType())) ||
          (reg.exec(node.getTitle())) ||
          reg.exec(node.nodeLabel.opcode) !== null);
      };

      const selection = this.searchNodes(filterFunction, e, onlyVisible);

      this.nodeSelectionHandler.select(selection, true, false);
      this.connectVisibleSelectedElements(this.state.selection);
      this.updateGraphVisibility();
      searchInput.blur();
      this.viewSelection();
      this.focusOnSvg();
    }
    e.stopPropagation();
  }

  public detachSelection(): SelectionStorage {
    return new SelectionStorage(this.state.selection.detachSelection());
  }

  public adaptSelection(rememberedSelection: SelectionStorage): SelectionStorage {
    if (!this.graph.nodeMap || !(rememberedSelection instanceof SelectionStorage)) {
      return new SelectionStorage();
    }

    for (const node of rememberedSelection.adaptedNodes) {
      this.graph.makeNodeVisible(node);
    }

    for (const [key, node] of rememberedSelection.nodes.entries()) {
      // Adding survived nodes (with the same id)
      const survivedNode = this.graph.nodeMap[key];
      if (survivedNode) {
        const key = this.state.selection.stringKey(survivedNode);
        rememberedSelection.adaptNode(key);
        this.graph.makeNodeVisible(key);
      }
      // Adding children of nodes
      const childNodes = this.graph.originNodesMap.get(key);
      if (childNodes?.length > 0) {
        for (const childNode of childNodes) {
          const key = this.state.selection.stringKey(childNode);
          rememberedSelection.adaptNode(key);
          this.graph.makeNodeVisible(key);
        }
      }
      // Adding ancestors of nodes
      const originStringKey = this.state.selection.originStringKey(node);
      if (originStringKey) {
        rememberedSelection.adaptNode(originStringKey);
        this.graph.makeNodeVisible(originStringKey);
      }
    }

    return rememberedSelection;
  }

  private initializeNodeSelectionHandler(): NodeSelectionHandler & ClearableHandler {
    const view = this;
    return {
      select: function (selectedNodes: Array<GraphNode>, selected: boolean,
                        scrollIntoView: boolean) {
        const locations = new Array<GenericPosition>();
        const nodes = new Set<string>();
        for (const node of selectedNodes) {
          if (node.nodeLabel.sourcePosition) {
            locations.push(node.nodeLabel.sourcePosition);
            nodes.add(node.identifier());
          }
          if (node.nodeLabel.bytecodePosition) {
            locations.push(node.nodeLabel.bytecodePosition);
            nodes.add(node.identifier());
          }
        }
        view.state.selection.select(selectedNodes, selected);
        view.broker.broadcastSourcePositionSelect(this, locations, selected, nodes);
        view.updateGraphVisibility();
      },
      clear: function () {
        view.state.selection.clear();
        view.broker.broadcastClear(this);
        view.updateGraphVisibility();
      },
      brokeredNodeSelect: function (nodeIds: Set<string>, selected: boolean) {
        if (!view.graph) return;
        const selection = view.graph.nodes(node => nodeIds.has(node.identifier())
          && (!view.state.hideDead || node.isLive()));
        view.state.selection.select(selection, selected);
        // Update edge visibility based on selection.
        for (const item of view.state.selection.selectedKeys()) {
          const node = view.graph.nodeMap[item];
          if (!node) continue;
          node.visible = true;
          node.inputs.forEach(edge => {
            edge.visible = edge.visible || edge.source.visible;
          });
          node.outputs.forEach(edge => {
            edge.visible = edge.visible || edge.target.visible;
          });
        }
        view.updateGraphVisibility();
      },
      brokeredClear: function () {
        view.state.selection.clear();
        view.updateGraphVisibility();
      }
    };
  }

  private createGraph(data: GraphPhase, rememberedSelection: SelectionStorage): SelectionStorage {
    this.graph = new Graph(data);
    this.graphLayout = new GraphLayout(this.graph);

    if (!this.state.cacheLayout ||
      this.graph.graphPhase.stateType == GraphStateType.NeedToFullRebuild) {
      this.updateGraphStateType(GraphStateType.NeedToFullRebuild);
      this.showControlAction(this);
    } else {
      this.showVisible();
    }

    const adaptedSelection = this.adaptSelection(rememberedSelection);

    this.graph.makeEdgesVisible();

    this.layoutGraph();
    this.updateGraphVisibility();
    return adaptedSelection;
  }

  private layoutGraph(): void {
    const layoutMessage = this.graph.graphPhase.stateType == GraphStateType.Cached
      ? "Layout graph from cache"
      : "Layout graph";

    console.time(layoutMessage);
    this.graphLayout.rebuild(this.state.showTypes);
    const extent = this.graph.redetermineGraphBoundingBox(this.state.showTypes);
    this.panZoom.translateExtent(extent);
    this.minScale();
    console.timeEnd(layoutMessage);
  }

  private appendInputAndOutputBubbles(svg: d3.Selection<SVGGElement, GraphNode, null, GraphNode>,
                                      node: GraphNode): void {
    const view = this;
    for (let i = 0; i < node.inputs.length; ++i) {
      const x = node.getInputX(i);
      const y = -C.DEFAULT_NODE_BUBBLE_RADIUS;
      svg.append("circle")
        .classed("filledBubbleStyle", node.inputs[i].isVisible())
        .classed("bubbleStyle", !node.inputs[i].isVisible())
        .attr("id", `ib,${node.inputs[i]}`)
        .attr("r", C.DEFAULT_NODE_BUBBLE_RADIUS)
        .attr("transform", `translate(${x},${y})`)
        .on("click", function (this: SVGCircleElement) {
          const components = this.id.split(",");
          const node = view.graph.nodeMap[components[3]];
          const edge = node.inputs[components[2]];
          const visible = !edge.isVisible();
          node.setInputVisibility(components[2], visible);
          d3.event.stopPropagation();
          view.updateGraphVisibility();
        });
    }
    if (node.outputs.length > 0) {
      const x = node.getOutputX();
      const y = node.getHeight(view.state.showTypes) + C.DEFAULT_NODE_BUBBLE_RADIUS;
      svg.append("circle")
        .classed("filledBubbleStyle", node.areAnyOutputsVisible()
          == OutputVisibilityType.AllNodesVisible)
        .classed("halFilledBubbleStyle", node.areAnyOutputsVisible()
          == OutputVisibilityType.SomeNodesVisible)
        .classed("bubbleStyle", node.areAnyOutputsVisible()
          == OutputVisibilityType.NoVisibleNodes)
        .attr("id", `ob,${node.id}`)
        .attr("r", C.DEFAULT_NODE_BUBBLE_RADIUS)
        .attr("transform", `translate(${x},${y})`)
        .on("click", node => {
          node.setOutputVisibility(node.areAnyOutputsVisible()
            == OutputVisibilityType.NoVisibleNodes);
          d3.event.stopPropagation();
          view.updateGraphVisibility();
        });
    }
  }

  private updateInputAndOutputBubbles(): void {
    const view = this;
    const graph = this.graph;
    const bubbles = this.visibleBubbles;
    bubbles.classed("filledBubbleStyle", function () {
      const components = this.id.split(",");
      if (components[0] === "ib") {
        const edge = graph.nodeMap[components[3]].inputs[components[2]];
        return edge.isVisible();
      }
      return graph.nodeMap[components[1]].areAnyOutputsVisible()
        == OutputVisibilityType.AllNodesVisible;
    }).classed("halfFilledBubbleStyle", function () {
      const components = this.id.split(",");
      if (components[0] === "ib") return false;
      return graph.nodeMap[components[1]].areAnyOutputsVisible()
        == OutputVisibilityType.SomeNodesVisible;
    }).classed("bubbleStyle", function () {
      const components = this.id.split(",");
      if (components[0] === "ib") {
        const edge = graph.nodeMap[components[3]].inputs[components[2]];
        return !edge.isVisible();
      }
      return graph.nodeMap[components[1]].areAnyOutputsVisible()
        == OutputVisibilityType.NoVisibleNodes;
    });
    bubbles.each(function () {
      const components = this.id.split(",");
      if (components[0] === "ob") {
        const from = graph.nodeMap[components[1]];
        const x = from.getOutputX();
        const y = from.getHeight(view.state.showTypes) + C.DEFAULT_NODE_BUBBLE_RADIUS;
        this.setAttribute("transform", `translate(${x},${y})`);
      }
    });
  }

  private attachSelection(selection: Set<string>): Array<GraphNode> {
    if (!(selection instanceof Set)) return new Array<GraphNode>();
    this.nodeSelectionHandler.clear();
    const selected = [
      ...this.graph.nodes(node =>
        selection.has(this.state.selection.stringKey(node))
        && (!this.state.hideDead || node.isLive()))
    ];
    this.nodeSelectionHandler.select(selected, true, false);
    return selected;
  }

  private viewSelection(): void {
    let minX;
    let maxX;
    let minY;
    let maxY;
    let hasSelection = false;
    this.visibleNodes.selectAll<SVGGElement, GraphNode>("g").each((node: GraphNode) => {
      if (this.state.selection.isSelected(node)) {
        hasSelection = true;
        minX = minX ? Math.min(minX, node.x) : node.x;
        maxX = maxX ? Math.max(maxX, node.x + node.getWidth()) : node.x + node.getWidth();
        minY = minY ? Math.min(minY, node.y) : node.y;
        maxY = maxY
          ? Math.max(maxY, node.y + node.getHeight(this.state.showTypes))
          : node.y + node.getHeight(this.state.showTypes);
      }
    });
    if (hasSelection) {
      this.viewGraphRegion(minX - C.NODE_INPUT_WIDTH, minY - 60,
        maxX + C.NODE_INPUT_WIDTH, maxY + 60);
    }
  }

  // Actions (handlers of toolbox menu and hotkeys events)
  private layoutAction(view: GraphView): void {
    view.updateGraphStateType(GraphStateType.NeedToFullRebuild);
    view.layoutGraph();
    view.updateGraphVisibility();
    view.viewWholeGraph();
    view.focusOnSvg();
  }

  private showAllAction(view: GraphView): void {
    for (const node of view.graph.nodes()) {
      node.visible = !view.state.hideDead || node.isLive();
    }
    view.graph.forEachEdge((edge: GraphEdge) => {
      edge.visible = edge.source.visible || edge.target.visible;
    });
    view.updateGraphVisibility();
    view.viewWholeGraph();
    view.focusOnSvg();
  }

  private showControlAction(view: GraphView): void {
    for (const node of view.graph.nodes()) {
      node.visible = node.cfg && (!view.state.hideDead || node.isLive());
    }
    view.graph.forEachEdge((edge: GraphEdge) => {
      edge.visible = edge.type === "control" && edge.source.visible && edge.target.visible;
    });
    view.showVisible();
  }

  private hideUnselectedAction(view: GraphView): void {
    for (const node of view.graph.nodes()) {
      if (!view.state.selection.isSelected(node)) {
        node.visible = false;
      }
    }
    view.updateGraphVisibility();
    view.focusOnSvg();
  }

  private hideSelectedAction(view: GraphView): void {
    for (const node of view.graph.nodes()) {
      if (view.state.selection.isSelected(node)) {
        node.visible = false;
      }
    }
    view.nodeSelectionHandler.clear();
    view.focusOnSvg();
  }

  private zoomSelectionAction(view: GraphView): void {
    view.viewSelection();
    view.focusOnSvg();
  }

  private toggleHideDeadAction(view: GraphView): void {
    view.state.hideDead = !view.state.hideDead;
    if (view.state.hideDead) {
      view.hideDead();
    } else {
      view.showDead();
    }
    const element = document.getElementById("toggle-hide-dead");
    element.classList.toggle("button-input-toggled", view.state.hideDead);
    view.focusOnSvg();
  }

  private toggleTypesAction(view: GraphView): void {
    view.state.showTypes = !view.state.showTypes;
    const element = document.getElementById("toggle-types");
    element.classList.toggle("button-input-toggled", view.state.showTypes);
    view.updateGraphVisibility();
    view.focusOnSvg();
  }

  private toggleLayoutCachingAction(view: GraphView): void {
    view.state.cacheLayout = !view.state.cacheLayout;
    const element = document.getElementById("toggle-cache-layout");
    element.classList.toggle("button-input-toggled", view.state.cacheLayout);
  }

  private hideDead(): void {
    for (const node of this.graph.nodes()) {
      if (!node.isLive()) {
        node.visible = false;
        this.state.selection.select([node], false);
      }
    }
    this.updateGraphVisibility();
  }

  private showDead(): void {
    for (const node of this.graph.nodes()) {
      if (!node.isLive()) {
        node.visible = true;
      }
    }
    this.updateGraphVisibility();
  }

  // Hotkeys handlers
  private selectAllNodes(): void {
    if (!d3.event.shiftKey) {
      this.state.selection.clear();
    }
    const allVisibleNodes = [...this.graph.nodes(node => node.visible)];
    this.state.selection.select(allVisibleNodes, true);
    this.updateGraphVisibility();
  }

  private selectOrigins(): void {
    const selection = new SelectionStorage();
    const origins = new Array<GraphNode>();
    let phase = this.phaseName;
    for (const node of this.state.selection) {
      const origin = node.nodeLabel.origin;
      if (origin && origin instanceof NodeOrigin) {
        phase = origin.phase;
        const node = this.graph.nodeMap[origin.identifier()];
        if (phase === this.phaseName && node) {
          origins.push(node);
        } else {
          selection.adaptNode(origin.identifier());
        }
      }
    }
    // Only go through phase reselection if we actually need
    // to display another phase.
    if (selection.isAdapted() && phase !== this.phaseName) {
      this.showPhaseByName(phase, selection);
    } else if (origins.length > 0) {
      this.nodeSelectionHandler.clear();
      this.nodeSelectionHandler.select(origins, true, false);
    }
  }
}
