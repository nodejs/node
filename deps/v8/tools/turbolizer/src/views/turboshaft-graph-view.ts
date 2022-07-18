// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as d3 from "d3";
import * as C from "../common/constants";
import { partial } from "../common/util";
import { MovableView } from "./movable-view";
import { TurboshaftGraphPhase } from "../phases/turboshaft-graph-phase/turboshaft-graph-phase";
import { SelectionBroker } from "../selection/selection-broker";
import { SelectionMap } from "../selection/selection";
import { TurboshaftGraphBlock, TurboshaftGraphBlockType } from "../phases/turboshaft-graph-phase/turboshaft-graph-block";
import { TurboshaftGraphNode } from "../phases/turboshaft-graph-phase/turboshaft-graph-node";
import { TurboshaftGraphEdge } from "../phases/turboshaft-graph-phase/turboshaft-graph-edge";
import { TurboshaftGraph } from "../turboshaft-graph";
import { TurboshaftGraphLayout } from "../turboshaft-graph-layout";
import { GraphStateType } from "../phases/graph-phase/graph-phase";
import { OutputVisibilityType } from "../node";
import { BlockSelectionHandler, ClearableHandler } from "../selection/selection-handler";

export class TurboshaftGraphView extends MovableView<TurboshaftGraph> {
  graphLayout: TurboshaftGraphLayout;
  blocksSelectionHandler: BlockSelectionHandler & ClearableHandler;
  visibleBlocks: d3.Selection<any, TurboshaftGraphBlock, any, any>;
  visibleNodes: d3.Selection<any, TurboshaftGraphNode, any, any>;
  visibleEdges: d3.Selection<any, TurboshaftGraphEdge<TurboshaftGraphBlock>, any, any>;
  visibleBubbles: d3.Selection<any, any, any, any>;
  blockDrag: d3.DragBehavior<any, TurboshaftGraphBlock, TurboshaftGraphBlock>;

  constructor(idOrContainer: string | HTMLElement, broker: SelectionBroker,
              showPhaseByName: (name: string) => void, toolbox: HTMLElement) {
    super(idOrContainer, broker, showPhaseByName, toolbox);

    this.state.selection = new SelectionMap((b: TurboshaftGraphBlock) => b.identifier());

    this.visibleBlocks = this.graphElement.append("g");
    this.visibleEdges = this.graphElement.append("g");
    this.visibleNodes = this.graphElement.append("g");

    this.blockDrag = d3.drag<any, TurboshaftGraphBlock, TurboshaftGraphBlock>()
      .on("drag",  (block: TurboshaftGraphBlock) => {
        block.x += d3.event.dx;
        block.y += d3.event.dy;
        this.updateVisibleBlocks();
      });
  }

  public initializeContent(data: TurboshaftGraphPhase, rememberedSelection: Map<string, any>):
    void {
    this.show();
    this.addImgInput("layout", "layout graph",
      partial(this.layoutAction, this));
    this.addImgInput("show-all", "show all blocks",
      partial(this.showAllBlocksAction, this));
    this.addToggleImgInput("toggle-properties", "toggle propetries",
      this.state.showProperties, partial(this.togglePropertiesAction, this));
    this.addToggleImgInput("toggle-cache-layout", "toggle saving graph layout",
      this.state.cacheLayout, partial(this.toggleLayoutCachingAction, this));

    this.phaseName = data.name;
    this.createGraph(data, rememberedSelection);

    this.viewWholeGraph();
    if (this.state.cacheLayout && data.transform) {
      this.svg.call(this.panZoom.transform, d3.zoomIdentity
        .translate(data.transform.x, data.transform.y)
        .scale(data.transform.scale));
    }
  }

  public updateGraphVisibility(): void {
    if (!this.graph) return;
    this.updateVisibleBlocks();
    this.visibleNodes = d3.selectAll(".turboshaft-node");
    this.visibleBubbles = d3.selectAll("circle");
    this.updateInlineNodes();
    this.updateInputAndOutputBubbles();
  }

  public svgKeyDown(): void {
  }

  public searchInputAction(searchInput: HTMLInputElement, e: Event, onlyVisible: boolean): void {
  }

  private createGraph(data: TurboshaftGraphPhase, selection) {
    this.graph = new TurboshaftGraph(data);
    this.graphLayout = new TurboshaftGraphLayout(this.graph);

    if (!this.state.cacheLayout ||
      this.graph.graphPhase.stateType == GraphStateType.NeedToFullRebuild) {
      this.updateGraphStateType(GraphStateType.NeedToFullRebuild);
      this.showAllBlocksAction(this);
    } else {
      this.showVisible();
    }

    this.layoutGraph();
    this.updateGraphVisibility();
  }

  private layoutGraph(): void {
    const layoutMessage = this.graph.graphPhase.stateType == GraphStateType.Cached
      ? "Layout graph from cache"
      : "Layout graph";

    console.time(layoutMessage);
    this.graphLayout.rebuild(this.state.showProperties);
    const extent = this.graph.redetermineGraphBoundingBox(this.state.showProperties);
    this.panZoom.translateExtent(extent);
    this.minScale(this.graph.width, this.graph.height);
    console.timeEnd(layoutMessage);
  }

  private updateVisibleBlocks(): void {
    const view = this;
    // select existing blocks
    const filteredBlocks = [
      ...this.graph.blocks(block => this.graph.isRendered() && block.visible)
    ];
    const allBlocks = view.visibleBlocks
      .selectAll<SVGGElement, TurboshaftGraphBlock>(".turboshaft-block");
    const selBlocks = allBlocks.data(filteredBlocks, block => block.toString());

    // remove old blocks
    selBlocks.exit().remove();

    // add new blocks
    const newBlocks = selBlocks
      .enter()
      .append("g")
      .classed("turboshaft-block", true)
      .classed("block", b => b.type == TurboshaftGraphBlockType.Block)
      .classed("merge", b => b.type == TurboshaftGraphBlockType.Merge)
      .classed("loop", b => b.type == TurboshaftGraphBlockType.Loop)
      .call(view.blockDrag);

    newBlocks
      .append("rect")
      .attr("rx", 35)
      .attr("ry", 35)
      .attr("width", block => block.getWidth())
      .attr("height", block => block.getHeight(view.state.showProperties));

    newBlocks.each(function (block: TurboshaftGraphBlock) {
      const svg = d3.select<SVGGElement, TurboshaftGraphBlock>(this);
      svg
        .append("text")
        .attr("text-anchor", "middle")
        .attr("x", block.getWidth() / 2)
        .classed("block-label", true)
        .append("tspan")
        .text(block.displayLabel);
      view.appendInlineNodes(svg, block);
      view.appendInputAndOutputBubbles(svg, block);
    });

    newBlocks.merge(selBlocks)
      .classed("selected", block => view.state.selection.isSelected(block))
      .attr("transform", block => `translate(${block.x},${block.y})`)
      .select("rect")
      .attr("height", block =>  block.getHeight(view.state.showProperties));
  }

  private appendInlineNodes(svg: d3.Selection<SVGGElement, TurboshaftGraphBlock, any, any>,
                      block: TurboshaftGraphBlock): void {
    const state = this.state;
    const graph = this.graph;
    const filteredNodes = [...block.nodes.filter(node => graph.isRendered() && node.visible)];
    const allNodes = svg.selectAll<SVGGElement, TurboshaftGraphNode>(".turboshaft-inline-node");
    const selNodes = allNodes.data(filteredNodes, node => node.toString());

    // remove old nodes
    selNodes.exit().remove();

    // add new nodes
    const newNodes = selNodes
      .enter()
      .append("g")
      .classed("turboshaft-node", true)
      .classed("inline-node", true);

    let nodeY = block.labelBox.height;
    const blockWidth = block.getWidth();
    newNodes.each(function (node: TurboshaftGraphNode) {
      const nodeSvg = d3.select(this);
      nodeSvg
        .attr("id", node.id)
        .append("text")
        .attr("dx", 25)
        .classed("inline-node-label", true)
        .attr("dy", nodeY)
        .append("tspan")
        .text(`${node.getInlineLabel()}[${node.getPropertiesTypeAbbreviation()}]`);
      nodeY += node.labelBox.height;
      if (node.properties) {
        nodeSvg
          .append("text")
          .attr("dx", 25)
          .classed("inline-node-properties", true)
          .attr("dy", nodeY)
          .append("tspan")
          .text(node.getReadableProperties(blockWidth))
          .append("title")
          .text(node.properties);
        nodeY += node.labelBox.height;
      }
    });

    newNodes.merge(selNodes)
      .classed("selected", node => state.selection.isSelected(node))
      .select("rect")
      .attr("height", node => node.getHeight(state.showProperties));
  }

  private updateInputAndOutputBubbles(): void {
    const view = this;
    const graph = this.graph;
    this.visibleBubbles.classed("filledBubbleStyle", function () {
      const components = this.id.split(",");
      if (components[0] === "ib") {
        return graph.blockMap[components[3]].inputs[components[2]].isVisible();
      }
      return graph.blockMap[components[1]].areAnyOutputsVisible()
        == OutputVisibilityType.AllNodesVisible;
    }).classed("halfFilledBubbleStyle", function () {
      const components = this.id.split(",");
      if (components[0] === "ib") return false;
      return graph.blockMap[components[1]].areAnyOutputsVisible()
        == OutputVisibilityType.SomeNodesVisible;
    }).classed("bubbleStyle", function () {
      const components = this.id.split(",");
      if (components[0] === "ib") {
        return !graph.blockMap[components[3]].inputs[components[2]].isVisible();
      }
      return graph.blockMap[components[1]].areAnyOutputsVisible()
        == OutputVisibilityType.NoVisibleNodes;
    });
    this.visibleBubbles.each(function () {
      const components = this.id.split(",");
      if (components[0] === "ob") {
        const from = graph.blockMap[components[1]];
        const x = from.getOutputX();
        const y = from.getHeight(view.state.showProperties) + C.DEFAULT_NODE_BUBBLE_RADIUS;
        this.setAttribute("transform", `translate(${x},${y})`);
      }
    });
  }

  private appendInputAndOutputBubbles(
    svg: d3.Selection<SVGGElement, TurboshaftGraphBlock, any, any>,
    block: TurboshaftGraphBlock): void {
    for (let i = 0; i < block.inputs.length; i++) {
      const x = block.getInputX(i);
      const y = -C.DEFAULT_NODE_BUBBLE_RADIUS;
      svg.append("circle")
        .classed("filledBubbleStyle", block.inputs[i].isVisible())
        .classed("bubbleStyle", !block.inputs[i].isVisible())
        .attr("id", `ib,${block.inputs[i].toString(i)}`)
        .attr("r", C.DEFAULT_NODE_BUBBLE_RADIUS)
        .attr("transform", `translate(${x},${y})`);
    }
    if (block.outputs.length > 0) {
      const x = block.getOutputX();
      const y = block.getHeight(this.state.showProperties) + C.DEFAULT_NODE_BUBBLE_RADIUS;
      svg.append("circle")
        .classed("filledBubbleStyle", block.areAnyOutputsVisible()
          == OutputVisibilityType.AllNodesVisible)
        .classed("halFilledBubbleStyle", block.areAnyOutputsVisible()
          == OutputVisibilityType.SomeNodesVisible)
        .classed("bubbleStyle",  block.areAnyOutputsVisible()
          == OutputVisibilityType.NoVisibleNodes)
        .attr("id", `ob,${block.id}`)
        .attr("r", C.DEFAULT_NODE_BUBBLE_RADIUS)
        .attr("transform", `translate(${x},${y})`);
    }
  }

  private updateInlineNodes(): void {
    const state = this.state;
    let totalHeight = 0;
    let blockId = 0;
    this.visibleNodes.each(function (node: TurboshaftGraphNode) {
      if (blockId != node.block.id) {
        blockId = node.block.id;
        totalHeight = 0;
      }
      totalHeight += node.getHeight(state.showProperties);
      const nodeSvg = d3.select(this);
      const nodeY = state.showProperties && node.properties
        ? totalHeight - node.labelBox.height
        : totalHeight;
      nodeSvg.select(".inline-node-label").attr("dy", nodeY);
      nodeSvg.select(".inline-node-properties").attr("visibility", state.showProperties
        ? "visible"
        : "hidden");
    });
  }

  // Actions (handlers of toolbox menu and hotkeys events)
  private layoutAction(view: TurboshaftGraphView): void {
    view.updateGraphStateType(GraphStateType.NeedToFullRebuild);
    view.layoutGraph();
    view.updateGraphVisibility();
    view.viewWholeGraph();
    view.focusOnSvg();
  }

  private showAllBlocksAction(view: TurboshaftGraphView): void {
    for (const node of view.graph.blocks()) {
      node.visible = true;
    }
    for (const edge of view.graph.blocksEdges()) {
      edge.visible = true;
    }
    view.showVisible();
  }

  private togglePropertiesAction(view: TurboshaftGraphView): void {
    view.state.showProperties = !view.state.showProperties;
    const element = document.getElementById("toggle-properties");
    element.classList.toggle("button-input-toggled", view.state.showProperties);
    view.updateGraphVisibility();
    view.focusOnSvg();
  }

  private toggleLayoutCachingAction(view: TurboshaftGraphView): void {
    view.state.cacheLayout = !view.state.cacheLayout;
    const element = document.getElementById("toggle-cache-layout");
    element.classList.toggle("button-input-toggled", view.state.cacheLayout);
  }
}
