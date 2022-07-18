import { TurboshaftGraph } from "./turboshaft-graph";
import { GraphStateType } from "./phases/phase";
import { TurboshaftLayoutType } from "./phases/turboshaft-graph-phase/turboshaft-graph-phase";

export class TurboshaftGraphLayout {
  graph: TurboshaftGraph;

  constructor(graph: TurboshaftGraph) {
    this.graph = graph;
  }

  public rebuild(showProperties: boolean): void {
    if (this.graph.graphPhase.stateType == GraphStateType.NeedToFullRebuild) {
      switch (this.graph.graphPhase.layoutType) {
        case TurboshaftLayoutType.Inline:
          this.inlineRebuild(showProperties);
          break;
        default:
          throw "Unsupported graph layout type";
      }
      this.graph.graphPhase.stateType = GraphStateType.Cached;
    }
    this.graph.graphPhase.rendered = true;
  }

  private inlineRebuild(showProperties): void {
    // Useless logic to simulate blocks coordinates (will be replaced in the future)
    let x = 0;
    let y = 0;
    for (const block of this.graph.blockMap) {
      block.x = x;
      block.y = y;
      y += block.getHeight(showProperties) + 50;
      if (y > 1800) {
        x += block.getWidth() * 1.5;
        y = 0;
      }
    }
  }
}
