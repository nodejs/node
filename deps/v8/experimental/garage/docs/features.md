# `garage` Feature Catalog & Complete Reference Guide

This document is an exhaustive, one-by-one reference of **every feature** implemented in `garage` — a terminal user interface (TUI) for V8 engineers to view, navigate, search, and diff compiler trace outputs.

Each feature entry explains:
1. **Purpose & What You Can See / Identify**: Why the feature exists, what compiler behavior it reveals, and what an engineer can diagnose with it.
2. **How to Enable**: Required `d8` command-line flags, GN build arguments, and CLI invocation shapes.
3. **Actions in Garage**: The exact keybindings, command-palette commands, and mouse interactions to invoke and control the feature once `garage` is running.

---

## Table of Contents

- [1. Trace Acquisition, Invocation Modes & Indexing](#1-trace-acquisition-invocation-modes--indexing)
  - [1.1. Interactive Pipeline Mode (`d8 ... | garage`)](#11-interactive-pipeline-mode-d8----garage)
  - [1.2. Wrapper / Live Streaming Mode (`garage -- d8 ...`)](#12-wrapper--live-streaming-mode-garage----d8-)
  - [1.3. Multi-File Memory-Mapped Mode (`garage a.log b.log`)](#13-multi-file-memory-mapped-mode-garage-alog-blog)
  - [1.4. Index-Time Function Regex Prefilter (`--function <regex>`)](#14-index-time-function-regex-prefilter---function-regex)
  - [1.5. Diagnostic & Debug Logging (`--debug`, `--log-file <path>`)](#15-diagnostic--debug-logging---debug---log-file-path)
  - [1.6. Zero-Drop Parse Resilience (`[unparsed]` Badge)](#16-zero-drop-parse-resilience-unparsed-badge)
- [2. High-Level Overview & UI Layout](#2-high-level-overview--ui-layout)
  - [2.1. Telemetry Header Bar](#21-telemetry-header-bar)
  - [2.2. Sidebar: Compilation List & Function Grouping (`c`)](#22-sidebar-compilation-list--function-grouping-c)
  - [2.3. Sidebar Toggling & Minimal Strip (`b`)](#23-sidebar-toggling--minimal-strip-b)
  - [2.4. Sidebar Regex Filtering (`f` / `:function <regex>`)](#24-sidebar-regex-filtering-f---function-regex)
  - [2.5. Viewport Line Wrapping (`w`) & Horizontal Scrolling (`<` / `>`)](#25-viewport-line-wrapping-w--horizontal-scrolling---)
- [3. Multi-Tier IR Graph Viewers](#3-multi-tier-ir-graph-viewers)
  - [3.1. Maglev IR Graph Viewer & Optimization Pipeline](#31-maglev-ir-graph-viewer--optimization-pipeline)
  - [3.2. TurboFan Sea-of-Nodes, Schedule & Instruction Sequence Viewer](#32-turbofan-sea-of-nodes-schedule--instruction-sequence-viewer)
  - [3.3. Turboshaft IR Viewer (`BLOCK` / `MERGE` / `LOOP`)](#33-turboshaft-ir-viewer-block--merge--loop)
  - [3.4. Turbolev Frontend Graph Viewer](#34-turbolev-frontend-graph-viewer)
- [4. Interactive Graph & Code Navigation](#4-interactive-graph--code-navigation)
  - [4.1. Semantic Syntax Highlighting & Box-Drawing Block Gutters](#41-semantic-syntax-highlighting--box-drawing-block-gutters)
  - [4.2. Interactive Def-Use & Dependency Chain Highlighting](#42-interactive-def-use--dependency-chain-highlighting)
  - [4.3. Input Definition Jump (`i`) & Consumer Cycling (`u`)](#43-input-definition-jump-i--consumer-cycling-u)
  - [4.4. Navigation History Jump Back / Forward (`Ctrl+O` / `Ctrl+I`)](#44-navigation-history-jump-back--forward-ctrlo--ctrli)
  - [4.5. Basic Block Folding (`Space`, `z`) & Block Header Walking (`[` / `]`)](#45-basic-block-folding-space-z--block-header-walking---)
  - [4.6. Basic Block Predecessor Cycling (`u` on Block Headers)](#46-basic-block-predecessor-cycling-u-on-block-headers)
- [5. Bytecode, Feedback Vectors & Disassembly Integration](#5-bytecode-feedback-vectors--disassembly-integration)
  - [5.1. Ignition Bytecode Array Viewer & Operand Cycling](#51-ignition-bytecode-array-viewer--operand-cycling)
  - [5.2. Inline Cache (IC) Feedback Vector Viewer](#52-inline-cache-ic-feedback-vector-viewer)
  - [5.3. Optimized Machine Code Disassembly & Pipeline Integration](#53-optimized-machine-code-disassembly--pipeline-integration)
  - [5.4. Assembly Branch Target Resolution & Labeled Offset Navigation](#54-assembly-branch-target-resolution--labeled-offset-navigation)
- [6. Lenses & Structural Graph Filtering (`:`)](#6-lenses--structural-graph-filtering-)
  - [6.1. Guard & Check Node Lens (`:checks`)](#61-guard--check-node-lens-checks)
  - [6.2. Control Flow & Phi Backbone Lens (`:phi`)](#62-control-flow--phi-backbone-lens-phi)
  - [6.3. Register Allocation Spill & Reload Traffic Lens (`:spill`)](#63-register-allocation-spill--reload-traffic-lens-spill)
  - [6.4. Megamorphic Feedback & Slow-Path IC Lens (`:megamorphic`)](#64-megamorphic-feedback--slow-path-ic-lens-megamorphic)
  - [6.5. Lens & Filter Clearing (`:clear`)](#65-lens--filter-clearing-clear)
- [7. Deoptimization, OSR & Event Timeline](#7-deoptimization-osr--event-timeline)
  - [7.1. Full-Run Optimization / Deoptimization / OSR Timeline (`Tab`)](#71-full-run-optimization--deoptimization--osr-timeline-tab)
  - [7.2. Deopt-Only Timeline Filter (`:deopts`)](#72-deopt-only-timeline-filter-deopts)
  - [7.3. Timeline-to-Compilation Correlation Jump (`Enter`)](#73-timeline-to-compilation-correlation-jump-enter)
  - [7.4. Verbose Deopt Frame Unwinding Dump](#74-verbose-deopt-frame-unwinding-dump)
  - [7.5. Inlining Decisions Modal Panel (`I`)](#75-inlining-decisions-modal-panel-i)
- [8. Diffing Engines](#8-diffing-engines)
  - [8.1. Structural Node-Identity Phase Diff (`d`)](#81-structural-node-identity-phase-diff-d)
  - [8.2. Cross-Run Function & Tier Diff (`D`, Dual-Run Mode)](#82-cross-run-function--tier-diff-d-dual-run-mode)
- [9. Splits, Source Alignment & View Layouts](#9-splits-source-alignment--view-layouts)
  - [9.1. Vertical (`v`) & Horizontal (`s`) Viewport Splits](#91-vertical-v--horizontal-s-viewport-splits)
  - [9.2. JavaScript Source Code Alignment Pane (`S`)](#92-javascript-source-code-alignment-pane-s)
  - [9.3. Bidirectional Source Line Alignment (`Enter` Cycling)](#93-bidirectional-source-line-alignment-enter-cycling)
  - [9.4. Interleaved Trace Annotation Folding (`t`)](#94-interleaved-trace-annotation-folding-t)
- [10. Search, Clipboard, Export & Customization](#10-search-clipboard-export--customization)
  - [10.1. Incremental Regex Viewport Search (`/`, `n`, `N`)](#101-incremental-regex-viewport-search--n-n)
  - [10.2. OSC 52 Terminal Clipboard Integration (`y`, `Y`, `:copy`)](#102-osc-52-terminal-clipboard-integration-y-y-copy)
  - [10.3. Formatted GitHub-Flavored Markdown Export (`E`, `:export <path>`)](#103-formatted-github-flavored-markdown-export-e-export-path)
  - [10.4. Full Mouse Interaction](#104-full-mouse-interaction)
  - [10.5. Fully Customizable TOML Keymap (`--config`, `config.toml`)](#105-fully-customizable-toml-keymap---config-configtoml)
  - [10.6. Dynamic Keymap Help Modal (`?`)](#106-dynamic-keymap-help-modal-)
  - [10.7. Command-Palette Quitting (`:q`, `:quit`)](#107-command-palette-quitting-q-quit)

---

## 1. Trace Acquisition, Invocation Modes & Indexing

### 1.1. Interactive Pipeline Mode (`d8 ... | garage`)
- **Purpose & What You Can See**: Allows streaming trace output from `d8` directly into `garage` without dumping gigabytes of text into terminal scrollback or writing large temporary files to disk. Unlike standard pipes, `garage` opens `/dev/tty` for interactive keyboard control so the UI remains fully responsive while data streams in.
- **How to Enable**: Pipe any `d8` trace invocation to `garage`:
  ```sh
  d8 --print-maglev-graphs bench.js | garage
  ```
- **Actions in Garage**:
  - Press `F` (`Action::ToggleFollow`) to toggle "follow mode" (automatically scroll to the end of the stream as new compilations arrive, similar to `tail -f`).
  - Navigate the sidebar items immediately as they are indexed.

### 1.2. Wrapper / Live Streaming Mode (`garage -- d8 ...`)
- **Purpose & What You Can See**: Spawns `d8` as an interactive child process owned by `garage`. Both stdout and stderr are merged in exact arrival order (the natural interleaving a terminal would show), preventing either stream from stalling behind OS pipe buffers. When `d8` exits, its exit code is appended as a visible note at the bottom of the trace. Quitting `garage` automatically terminates the child process, and signal handlers (`SIGINT`, `SIGTERM`, `SIGHUP`) ensure terminal state is restored before exiting.
- **How to Enable**: Use `--` to separate `garage` flags from the wrapped command:
  ```sh
  garage -- d8 --print-maglev-graphs bench.js
  ```
- **Actions in Garage**:
  - Observe compilations appear live in the sidebar.
  - Scroll to the bottom of the trace (`G`) when execution finishes to see the exit status banner (`[Child exited with status 0]`).

### 1.3. Multi-File Memory-Mapped Mode (`garage a.log b.log`)
- **Purpose & What You Can See**: Opens one or more saved trace log files instantly using memory-mapped I/O (`mmap`), opening a 500 MB trace in ~0.5 seconds. Allows loading multiple trace files into a single session and quickly cycling between them without restarting the tool.
- **How to Enable**: Pass one or more file paths as positional arguments:
  ```sh
  garage trace1.log trace2.log trace3.log
  ```
- **Actions in Garage**:
  - Press `}` (`Action::NextSource`) to switch between loaded trace files. The sidebar, telemetry bar, and viewport will update to reflect the currently active file.

### 1.4. Index-Time Function Regex Prefilter (`--function <regex>`)
- **Purpose & What You Can See**: Restricts indexing so that only compilations whose JavaScript function name matches `<regex>` are retained in memory. This provides an aggressive memory bound when opening massive traces (such as 1.5 GB fuzzer dumps or long-running server traces), ensuring lightning-fast startup and uncluttered navigation.
- **How to Enable**: Pass `--function <regex>` on the command line:
  ```sh
  garage --function '^process' trace.log
  d8 --print-maglev-graphs bench.js | garage --function 'MyFunc'
  ```
- **Actions in Garage**:
  - The sidebar and timeline will display only compilations matching the regular expression. (Filtered-out compilations will be noted in timeline correlation messages if referenced by deopt events).

### 1.5. Diagnostic & Debug Logging (`--debug`, `--log-file <path>`)
- **Purpose & What You Can See**: Records internal diagnostics, indexing statistics, parser execution times, and error backtraces to a separate log file for diagnosing `garage` itself without disrupting the TUI screen.
- **How to Enable**: Pass `--debug` and optionally specify a custom log file path (defaults to `garage.log`):
  ```sh
  garage --debug --log-file /tmp/garage-debug.log trace.log
  ```
- **Actions in Garage**:
  - Run your session normally; open `/tmp/garage-debug.log` in a separate terminal to inspect internal diagnostics.

### 1.6. Zero-Drop Parse Resilience (`[unparsed]` Badge)
- **Purpose & What You Can See**: Guarantees that `garage` never drops or hides output. If V8 prints unrecognized syntax, future format changes occur, or concurrent threads interleave text, `garage` classifies the unparsed lines as a raw text section labeled with an `[unparsed]` badge. This ensures no data is ever lost.
- **How to Enable**: Enabled automatically on all traces.
- **Actions in Garage**:
  - Look for items labeled `[unparsed]` in the sidebar.
  - Press `Enter` on an `[unparsed]` item to inspect the raw un-demultiplexed or unrecognized text in the viewport.

---

## 2. High-Level Overview & UI Layout

### 2.1. Telemetry Header Bar
- **Purpose & What You Can See**: Displays high-level telemetry across the entire trace at the top of the terminal:
  - Total compilation count broken down by tier (`Maglev`, `TurboFan`, `Turbolev`, `Ignition`).
  - Total OSR (On-Stack Replacement) count and Deoptimization event count.
  - Detected V8 version (automatically inferred from the vocabulary of phase banners).
  - In dual-run mode (`garage a.log b.log`), displays both runs' headline stats side by side.
- **How to Enable**: Always active whenever a trace is loaded.
- **Actions in Garage**:
  - Constantly visible at the top of the UI; updates automatically when switching active files (`}`).

### 2.2. Sidebar: Compilation List & Function Grouping (`c`)
- **Purpose & What You Can See**: Acts as the primary table of contents, listing every compiled function with its name, compiler tier, and compilation ordinal (`name · tier · #ordinal`), decorated with `[OSR]` or `[unparsed]` badges. Allows viewing functions in stream chronological order or grouped by function identity (keyed by SharedFunctionInfo address).
- **How to Enable**: Run any V8 tracing command that emits compilation output.
- **Actions in Garage**:
  - `j` / `k` or `↑` / `↓`: Move sidebar selection up or down.
  - `Enter` (`Action::Select`): Expand/collapse a compilation or select an individual phase to load into the viewport.
  - `c` (`Action::ToggleGrouping`): Toggle between chronological order (as emitted by `d8`) and grouped-by-function order.

### 2.3. Sidebar Toggling & Minimal Strip (`b`)
- **Purpose & What You Can See**: Allows hiding the sidebar to maximize viewport width for wide IR graphs or split diffs. When hidden, a slim 2-column strip with an accent arrow (`▸`) remains at mid-height as a visual indicator without consuming valuable horizontal space.
- **How to Enable**: Available at all times.
- **Actions in Garage**:
  - `b` (`Action::ToggleSidebar`): Hide or show the sidebar.
  - When hidden, press `h`, click on the `▸` strip with the mouse, or press `Tab` (which opens the timeline) to restore the sidebar.

### 2.4. Sidebar Regex Filtering (`f` / `:function <regex>`)
- **Purpose & What You Can See**: Narrows the sidebar dynamically to show only compilations whose function name matches a user-provided regular expression, helping isolate specific hot methods in large traces.
- **How to Enable**: Active in any loaded trace session.
- **Actions in Garage**:
  - Press `f` (`Action::Filter`) or type `:function <regex>` in the command palette.
  - Type your regex in the bottom prompt and press `Enter`.
  - Press `Esc` or type `:clear` in the command palette to remove the filter.

### 2.5. Viewport Line Wrapping (`w`) & Horizontal Scrolling (`<` / `>`)
- **Purpose & What You Can See**: Accommodates very wide IR node lines, deeply indented basic block structures, or long assembly instructions by letting you toggle between soft line-wrapping and horizontal scrolling.
- **How to Enable**: Available in any viewport pane.
- **Actions in Garage**:
  - `w` (`Action::ToggleWrap`): Toggle line wrapping on/off.
  - `<` / `>` (`Action::ScrollLeft` / `Action::ScrollRight`): Scroll horizontally when line wrapping is disabled.
  - `j` / `k`, `g` / `G`: Scroll vertically through the viewport.
  - `PgUp` / `PgDn`, `Ctrl+D` / `Ctrl+U`: Scroll vertically by page/half-page through the viewport (when pressed in the sidebar, immediately switches focus to the viewport).

---

## 3. Multi-Tier IR Graph Viewers

### 3.1. Maglev IR Graph Viewer & Optimization Pipeline
- **Purpose & What You Can See**: Renders Maglev intermediate representation graphs across all compiler phases (`V8.Maglev`, `V8.MaglevOptimized`, etc.). Displays basic block headers, phis, IR nodes, register allocations, inputs, and uses.
- **How to Enable**:
  - Requires a V8 build with `V8_ENABLE_MAGLEV_GRAPH_PRINTER` enabled (default in `debug` builds; check GN args for `release`).
  - Standard command:
    ```sh
    d8 --print-maglev-graphs bench.js | garage
    ```
  - Recommended clean command (prevents multi-threaded interleaving):
    ```sh
    d8 --no-concurrent-recompilation --predictable --print-maglev-graphs bench.js | garage
    ```
  - For stable node IDs across runs (strips volatile `[ML:<id>]` prefixes):
    ```sh
    d8 --no-trace-with-compilation-id --print-maglev-graphs bench.js | garage
    ```
- **Actions in Garage**:
  - Select a Maglev compilation in the sidebar, press `Enter` to expand its phases, and select any phase to view its IR graph.

### 3.2. TurboFan Sea-of-Nodes, Schedule & Instruction Sequence Viewer
- **Purpose & What You Can See**: Renders all stages of the TurboFan optimization pipeline:
  - **Sea-of-Nodes IR**: Identifies nodes, value/effect/control inputs, and opcode parameters (`#17:Op[params](#2:...)`).
  - **Schedule**: Displays structured control-flow blocks (`--- BLOCK B1 id1 ---`), node schedules, and block transitions (`-> B1`).
  - **Instruction Sequence**: Displays virtual register assignments (`vN`) with full definition and use tracking across basic blocks.
- **How to Enable**: Pass `--trace-turbo-graph` to `d8`:
  ```sh
  d8 --trace-turbo-graph bench.js | garage
  ```
- **Actions in Garage**:
  - Select a TurboFan compilation in the sidebar and navigate through its sea-of-nodes, schedule, or instruction sequence phases in the viewport.

### 3.3. Turboshaft IR Viewer (`BLOCK` / `MERGE` / `LOOP`)
- **Purpose & What You Can See**: Renders Turboshaft IR graphs with explicit control-flow block headers (`BLOCK`, `MERGE`, `LOOP`), memory load/store operations, and block targets.
- **How to Enable**: Emitted as part of `--trace-turbo-graph` in modern V8 builds where Turboshaft is enabled in the optimization pipeline:
  ```sh
  d8 --trace-turbo-graph bench.js | garage
  ```
- **Actions in Garage**:
  - Expand a Turboshaft phase in the sidebar; inspect structured control-flow headers and navigate operands using def-use bindings.

### 3.4. Turbolev Frontend Graph Viewer
- **Purpose & What You Can See**: Visualizes Maglev IR graphs as they are imported and processed by the TurboFan/Turboshaft pipeline when using the Turbolev frontend.
- **How to Enable**: Pass `--turbolev` and `--print-turbolev-frontend`:
  ```sh
  d8 --turbolev --print-turbolev-frontend bench.js | garage
  ```
- **Actions in Garage**:
  - Look for compilations labeled with the `Turbolev` tier in the sidebar and inspect frontend translation phases.

---

## 4. Interactive Graph & Code Navigation

### 4.1. Semantic Syntax Highlighting & Box-Drawing Block Gutters
- **Purpose & What You Can See**: Automatically parses and color-codes IR nodes and listing rows by semantic role:
  - **Red**: Guard nodes, checks, assertions, and deoptimization triggers.
  - **Blue**: Control-flow jumps, branches, switches, and block targets.
  - **Magenta**: Constants and immediates.
  - **Yellow**: Phis and control/data merges.
  - **Dim**: Interleaved source bytecode markers.
  - **Box-Drawing Gutters**: Gutter columns draw continuous vertical box-drawing lines for basic block edges, assigning a unique color to each edge column so overlapping control flow is easy to trace visually.
- **How to Enable**: Works automatically on all parsed IR graphs, bytecode listings, and assembly listings.
- **Actions in Garage**:
  - Navigate the viewport; gutter lines and node syntax colors update automatically.

### 4.2. Interactive Def-Use & Dependency Chain Highlighting
- **Purpose & What You Can See**: Instantly reveals data-flow dependencies without text searching. When you position the cursor on any node, bytecode instruction, or assembly line:
  - The **Definition** of the node is highlighted.
  - All **Inputs (Operands/References)** of the cursor node are highlighted.
  - All **Consumers (Uses)** that reference the cursor node are highlighted in distinct colors.
- **How to Enable**: Active on all parsed IR graphs, bytecode arrays, and disassembled machine code.
- **Actions in Garage**:
  - Move the cursor (`j` / `k` or mouse click) over any node or row in the viewport to see live def-use highlighting.

### 4.3. Input Definition Jump (`i`) & Consumer Cycling (`u`)
- **Purpose & What You Can See**: Provides IDE-style jump-to-definition and find-references navigation across graphs, bytecode, and assembly:
  - `i` jumps to the definition of the input/operand under the cursor. If a node has multiple inputs, repeatedly pressing `i` cycles through each input's definition.
  - `u` cycles through all consumers (uses) of the node under the cursor.
- **How to Enable**: Available on all parsed nodes, bytecode slots, and assembly branch targets.
- **Actions in Garage**:
  - Place cursor on a node or operand and press `i` (`Action::JumpToInput`) to jump to its defining row.
  - Press `u` (`Action::CycleConsumers`) to cycle through all rows that consume/reference the current node.

### 4.4. Navigation History Jump Back / Forward (`Ctrl+O` / `Ctrl+I`)
- **Purpose & What You Can See**: Maintains a persistent navigation history stack as you jump between nodes, basic blocks, bytecode offsets, or timeline deopt events.
- **How to Enable**: Active automatically during any navigation jump.
- **Actions in Garage**:
  - Press `Ctrl+O` (`Action::JumpBack`) to return to your previous location before the jump.
  - Press `Ctrl+I` (`Action::JumpForward`) to move forward in navigation history.

### 4.5. Basic Block Folding (`Space`, `z`) & Block Header Walking (`[` / `]`)
- **Purpose & What You Can See**: Simplifies large control-flow graphs by allowing you to collapse basic blocks into single-line summary headers, or rapidly skip between basic block leaders.
- **How to Enable**: Available in all parsed graphs, schedules, bytecode arrays, and machine disassembly views.
- **Actions in Garage**:
  - `Space` (`Action::FoldBlock`): Fold or unfold the basic block under the cursor.
  - `z` (`Action::FoldAllBlocks`): Fold all basic blocks in the current viewport (or unfold them all if already folded).
  - `[` / `]` (`Action::PrevBlock` / `Action::NextBlock`): Jump directly to the previous or next basic block header.

### 4.6. Basic Block Predecessor Cycling (`u` on Block Headers)
- **Purpose & What You Can See**: Answers the "who jumps here?" question at loop headers and merge points. When positioned on a basic block header (e.g. `--- BLOCK B2 ---` or a branch destination label), pressing `u` cycles through all **predecessor blocks** whose jumps or branches target this block.
- **How to Enable**: Available on block headers in Maglev, Turboshaft, schedules, bytecode, and disassembly listings.
- **Actions in Garage**:
  - Move the cursor to a basic block header line and press `u` (`Action::CycleConsumers`) to jump through all predecessor jump/branch instructions targeting this block.

---

## 5. Bytecode, Feedback Vectors & Disassembly Integration

### 5.1. Ignition Bytecode Array Viewer & Operand Cycling
- **Purpose & What You Can See**: Parses generated Ignition bytecode arrays (`----- Bytecode array -----` or `--print-bytecode`) as a **single contiguous display** (including constant pool, handler table, and source position table), styling mnemonics, jump targets (`(0x... @ N)`), switch tables, and constant pool references (`[N: ...]`).
- **How to Enable**:
  - Automatically parsed when printed ahead of Maglev/TurboFan graphs.
  - Standalone Ignition invocation:
    ```sh
    d8 --print-bytecode bench.js | garage
    ```
- **Actions in Garage**:
  - On a bytecode row, press `i` to cycle through its referenced operands (jump target offset, feedback vector slot, or constant pool entry).
  - Press `u` on a jump target row to cycle through all bytecode instructions branching to it.
  - Use `[` / `]` to navigate basic blocks within the bytecode listing.

### 5.2. Inline Cache (IC) Feedback Vector Viewer
- **Purpose & What You Can See**: Parses the feedback vector (`[FeedbackVector]`) printed below bytecode arrays and highlights inline cache (IC) feedback slots by polymorphism severity:
  - **Green**: `MONOMORPHIC` slots.
  - **Yellow**: `POLYMORPHIC` slots.
  - **Red**: `MEGAMORPHIC` slots.
- **How to Enable**: Emitted alongside bytecode arrays in Maglev/TurboFan traces or via `--print-bytecode`.
- **Actions in Garage**:
  - Scroll below the bytecode array to inspect feedback slots.
  - Place cursor on a feedback slot (`- slot #N ...`) and press `u` to cycle through all bytecode instructions referencing that slot.

### 5.3. Optimized Machine Code Disassembly & Pipeline Integration
- **Purpose & What You Can See**: Merges machine assembly code dumps (`Optimized code`, `Instructions`, `Inlined functions`, `RelocInfo`, `Deoptimization Input Data`) directly into the compilation pipeline that produced them as its final phases. This gives a unified sidebar journey from JS source -> bytecode -> IR graphs -> machine assembly without fragmented standalone sections.
- **How to Enable**:
  - Pass `--print-opt-code` alongside graph flags (requires `v8_enable_disassembler = true` in your V8 build GN args):
    ```sh
    d8 --print-maglev-graphs --print-opt-code bench.js | garage
    ```
  - Also supports `--print-maglev-code` or `--print-code`:
    ```sh
    d8 --print-maglev-graphs --print-maglev-code bench.js | garage
    ```
- **Actions in Garage**:
  - Expand a compilation in the sidebar and select the `Instructions` or `Optimized code` phase at the end of the phase list.

### 5.4. Assembly Branch Target Resolution & Labeled Offset Navigation
- **Purpose & What You Can See**: Resolves machine branch destination offsets (both x64 `<+0x...>` offsets and arm64 `(addr 0x...)` absolutes), marks destination addresses as labeled basic block leaders, and enables IDE-style jump navigation across machine instructions.
- **How to Enable**: Active on all disassembled `Instructions` phases.
- **Actions in Garage**:
  - Press `i` on a jump/branch instruction to jump directly to its destination address row.
  - Press `u` on a labeled destination row to cycle through all branch instructions jumping to it.
  - Press `[` / `]` to walk through labeled branch destination leaders.

---

## 6. Lenses & Structural Graph Filtering (`:`)

Lenses filter the modeled IR or bytecode viewport down to a structural skeleton (banner headers and basic block headers) plus only lines matching specific compiler phenomena. A live match count is displayed in the footer status line.

*(Note: The command palette accepts any unambiguous command prefix—for example, `:che` for `:checks`, `:exp` for `:export`, `:fun` for `:function`. If a prefix is ambiguous, such as `:c`, `garage` displays all matching candidates in the status line).*

### 6.1. Guard & Check Node Lens (`:checks`)
- **Purpose & What You Can See**: Isolates all guard nodes (`Check*`, `Assert*`, `*Deopt*`), showing exactly where the compiler inserted runtime safety checks and potential deoptimization bailouts.
- **How to Enable**: Open any IR graph phase in the viewport.
- **Actions in Garage**:
  - Press `:` to open the command palette, type `checks`, and press `Enter` (`Tab` auto-completes).

### 6.2. Control Flow & Phi Backbone Lens (`:phi`)
- **Purpose & What You Can See**: Isolates the control-flow and SSA phi backbone of the graph, showing only basic block leaders, jumps, branches, and `Phi` / `Res/LoopPhi` nodes.
- **How to Enable**: Open any IR graph phase in the viewport.
- **Actions in Garage**:
  - Press `:`, type `phi`, and press `Enter`.

### 6.3. Register Allocation Spill & Reload Traffic Lens (`:spill`)
- **Purpose & What You Can See**: Isolates register allocator spill and reload traffic (such as stack slot moves, spills, and reloads), helping diagnose register pressure and allocator inefficiency.
- **How to Enable**: Open an IR graph phase after register allocation (e.g. `V8.MaglevRegisterAllocation` or TurboFan instruction sequences).
- **Actions in Garage**:
  - Press `:`, type `spill`, and press `Enter`.

### 6.4. Megamorphic Feedback & Slow-Path IC Lens (`:megamorphic`)
- **Purpose & What You Can See**: Isolates all megamorphic feedback vector slots and slow-path inline caches in bytecode or IR graphs, highlighting optimization blockers.
- **How to Enable**: Open any bytecode listing or IR graph phase.
- **Actions in Garage**:
  - Press `:`, type `megamorphic`, and press `Enter`.

### 6.5. Lens & Filter Clearing (`:clear`)
- **Purpose & What You Can See**: Clears any active lens or timeline filter and restores the complete, unfiltered viewport. (Note: If you navigate via `i`/`u` to a node hidden by an active lens, `garage` automatically clears the lens so the jump succeeds).
- **How to Enable**: Active whenever a lens or filter is applied.
- **Actions in Garage**:
  - Press `:`, type `clear`, and press `Enter` (or switch to a different compilation).

---

## 7. Deoptimization, OSR & Event Timeline

### 7.1. Full-Run Optimization / Deoptimization / OSR Timeline (`Tab`)
- **Purpose & What You Can See**: Swaps the sidebar compilation list for a chronological **event timeline** showing every optimization (`--trace-opt`), deoptimization (`--trace-deopt`), and OSR (`--trace-osr`) event in stream order. Events are color-coded by severity:
  - **Red**: Deoptimizations.
  - **Green**: Successful optimizations / compilations.
  - **Yellow**: OSR (On-Stack Replacement) events.
- **How to Enable**: Pass event tracing flags to `d8`:
  ```sh
  d8 --print-maglev-graphs --trace-opt --trace-deopt --trace-osr app.js | garage
  ```
- **Actions in Garage**:
  - Press `Tab` (`Action::ToggleTimeline`) or type `:timeline` in the command palette to toggle between the compilation list and the timeline view.
  - Select any event in the timeline: the viewport displays the corresponding bailout block or event summary.

### 7.2. Deopt-Only Timeline Filter (`:deopts`)
- **Purpose & What You Can See**: Narrows the timeline view to show only deoptimization events, displaying the total deopt count in the status line.
- **How to Enable**: Open the timeline view (`Tab`).
- **Actions in Garage**:
  - Press `:`, type `deopts`, and press `Enter`.
  - Press `:`, type `clear` (or press `Tab` twice) to restore all timeline events.

### 7.3. Timeline-to-Compilation Correlation Jump (`Enter`)
- **Purpose & What You Can See**: Correlates a runtime deoptimization event to the exact compiler phase that generated the code. Pressing `Enter` on a deopt event automatically locates the matching compilation (using `SFI`, `tier`, and `ordinal` rules) and jumps the viewport to the exact bytecode offset where the bailout occurred. If an event cannot be correlated honestly, `garage` displays a clear status message instead of guessing.
- **How to Enable**: Requires a trace with both `--trace-deopt` and graph output (`--print-maglev-graphs`).
- **Actions in Garage**:
  - In the timeline (`Tab`), select a deopt event row and press `Enter`.
  - Press `Ctrl+O` (`Action::JumpBack`) to jump back from the compilation to the timeline event row.

### 7.4. Verbose Deopt Frame Unwinding Dump
- **Purpose & What You Can See**: Parses and formats verbose deoptimization frame unwinding dumps, showing precise register-to-node-to-location mappings (`register -> node -> location`) across virtual and physical stack frames during deoptimization.
- **How to Enable**: Pass `--print-maglev-deopt-verbose` (or `--trace-deopt-verbose`, which implies it):
  ```sh
  d8 --print-maglev-graphs --print-maglev-deopt-verbose bench.js | garage
  ```
- **Actions in Garage**:
  - Select a deopt event in the timeline (`Tab`) to view the styled frame-unwinding dump in the viewport.

### 7.5. Inlining Decisions Modal Panel (`I`)
- **Purpose & What You Can See**: Aggregates all inlining decisions (`⚡ INLINE` / `❌ SKIP`) recorded for the currently selected compilation into an interactive modal panel, showing the exact candidate function name and V8's reason for inlining or skipping.
- **How to Enable**: Pass `--trace-maglev-inlining` alongside graph printing flags:
  ```sh
  d8 --trace-maglev-inlining --print-maglev-graphs bench.js | garage
  ```
- **Actions in Garage**:
  - While viewing a compilation, press `I` (`Action::InliningPanel`) to open the inlining modal.
  - Use `j` / `k` to navigate the list of inlining decisions.
  - Press `Enter` on any decision to close the modal and jump directly to the decision line in the trace viewport.

---

## 8. Diffing Engines

### 8.1. Structural Node-Identity Phase Diff (`d`)
- **Purpose & What You Can See**: Compares two IR graph phases using a **node-identity based structural diff** rather than a naive line-by-line text diff. It matches nodes by V8 node ID (`nN`) within a compilation or by structural hash (opcode + canonically renumbered inputs) across compilations.
  - Ignores register reallocations, schedule ID changes, and use-count decorations (`~ changed` is flagged only when opcode or actual input IDs change).
  - Identity replacements (`nA: Identity [nB]`) render as `→ n5 replaced by n12`, and downstream consumers are reported as rerouted rather than modified.
  - Basic blocks that moved appear as `≈ moved` rather than deleted+added.
  - **Gutter Symbols & Row Colors**:
    - `+` (Green): Node added.
    - `−` (Red): Node removed.
    - `~` (Yellow): Node changed (opcode or actual inputs modified).
    - `→` (Blue): Node replaced or rerouted via Identity node.
    - `≈` (Cyan): Node or basic block moved.
- **How to Enable**: Available on any IR graph with two or more phases.
- **Actions in Garage**:
  - On a **phase row** in the sidebar: Press `d` (`Action::Diff`) to diff that phase against the *previous* graph phase.
  - On a **compilation row** in the sidebar: Press `d` to diff the *first* graph phase against the *last* graph phase.
  - With a **split viewport** open (`v` / `s`): Press `d` to diff whatever two phases are displayed in the two panes (including phases from different compilations of the same function).
  - Press `d` again to exit diff mode.
  - Press `Y` (`Action::YankSection`) or `E` (`Action::Export`) while in diff mode to copy or export the aligned side-by-side diff table with gutter symbols.

### 8.2. Cross-Run Function & Tier Diff (`D`, Dual-Run Mode)
- **Purpose & What You Can See**: Compares the same JavaScript function and compiler tier across two different `d8` execution traces (e.g. before vs. after a CL, across different V8 versions, or across different command-line flags). Uses structural node hashing so that differences in memory addresses or IDs do not cause false positives.
- **How to Enable**: Load two traces into `garage`:
  ```sh
  garage before.log after.log
  ```
  *(Tip: Use `--no-trace-with-compilation-id --no-concurrent-recompilation --predictable` when generating traces for cleanest comparison).*
- **Actions in Garage**:
  - Select a function compilation in the sidebar and press `D` (`Action::DualDiff`).
  - `garage` aligns the left pane with the first trace and the right pane with the second trace, diffing the same-named phase (or falling back to the last graph phase on both sides if phase banners were renamed across V8 versions).
  - Press `d` or `D` to exit dual-run diff mode.

---

## 9. Splits, Source Alignment & View Layouts

### 9.1. Vertical (`v`) & Horizontal (`s`) Viewport Splits
- **Purpose & What You Can See**: Splits the viewport into two independent panes so you can simultaneously view two different phases of a compilation, compare IR against machine disassembly, or inspect two different functions side by side.
- **How to Enable**: Available at all times.
- **Actions in Garage**:
  - `v` (`Action::SplitVertical`): Open a vertical split (left and right panes).
  - `s` (`Action::SplitHorizontal`): Open a horizontal split (top and bottom panes).
  - `Ctrl+W` (`Action::OtherPane`), `h` / `l`, or left-click: Switch focus between open panes. Each pane maintains its own sidebar selection.
  - Press `v` or `s` again while focused on a split pane to close it.

### 9.2. JavaScript Source Code Alignment Pane (`S`)
- **Purpose & What You Can See**: Opens a dedicated JavaScript source pane on the right half of the screen showing the compilation's script text with syntax highlighting and line numbers.
  - Resolves script files from the path printed in the SFI context (relative to trace dir or d8 cwd, capped at 8 MB).
  - **Fallback**: If the script file is not found on disk, automatically falls back to the compilation's own `Raw source` block from the trace, aligning coordinates through `source_position = N`.
- **How to Enable**: Available on any compilation with script position metadata.
- **Actions in Garage**:
  - Press `S` (`Action::ToggleSourcePane`) to open or close the source alignment pane.
  - As you move the cursor in the trace viewport, the corresponding JavaScript script line in the source pane is automatically highlighted and centered.

### 9.3. Bidirectional Source Line Alignment (`Enter` Cycling)
- **Purpose & What You Can See**: Lets you navigate from JavaScript source code directly to the corresponding IR nodes, bytecode instructions, or assembly instructions that implement that script line.
- **How to Enable**: Open the source alignment pane (`S`).
- **Actions in Garage**:
  - Press `Ctrl+W` (or `l` / left-click) to focus the JavaScript source pane.
  - Move the cursor to any JavaScript source line: aligned rows in the trace viewport highlight dynamically.
  - Press `Enter` on a JS script line to cycle the viewport cursor through all trace rows (bytecode, IR nodes, machine instructions) generated from that line.
  - Press `Ctrl+O` (`Action::JumpBack`) to return to your previous location.

### 9.4. Interleaved Trace Annotation Folding (`t`)
- **Purpose & What You Can See**: Cleans up viewport clutter by folding or unfolding interleaved V8 trace annotations, progress notes, and non-graph noise that appear inside a compilation phase body.
- **How to Enable**: Present whenever non-graph annotations occur inside a section.
- **Actions in Garage**:
  - Press `t` (`Action::ToggleAnnotations`) to fold or unfold interleaved trace annotations.

---

## 10. Search, Clipboard, Export & Customization

### 10.1. Incremental Regex Viewport Search (`/`, `n`, `N`)
- **Purpose & What You Can See**: Performs incremental regular expression searching within the currently displayed viewport section or graph.
- **How to Enable**: Available in any loaded view.
- **Actions in Garage**:
  - `/` (`Action::Search`): Open the search prompt in the footer. Type a regex pattern and press `Enter`.
  - `n` (`Action::SearchNext`): Jump to the next matching line in the viewport.
  - `N` (`Action::SearchPrev`): Jump to the previous matching line in the viewport.

### 10.2. OSC 52 Terminal Clipboard Integration (`y`, `Y`, `:copy`)
- **Purpose & What You Can See**: Copies lines or entire formatted sections directly to your system clipboard using **OSC 52 escape sequences**. Because OSC 52 operates over standard terminal I/O, copying works seamlessly across SSH sessions, `tmux`, and remote desktop containers without requiring X11 or Wayland forwarding.
- **How to Enable**: Works automatically in any terminal supporting OSC 52 (e.g. xterm, WezTerm, Alacritty, Terminal.app, iTerm2, gnome-terminal).
- **Actions in Garage**:
  - `y` (`Action::Yank`): Copy the single line under the cursor to the clipboard.
  - `Y` (`Action::YankSection`) or `:copy` in the command palette: Copy the entire selected section (or diff view) to the clipboard.

### 10.3. Formatted GitHub-Flavored Markdown Export (`E`, `:export <path>`)
- **Purpose & What You Can See**: Exports the currently active viewport section or diff table to a clean GitHub-flavored Markdown file on disk, preserving gutter symbols, column alignment, and headers.
- **How to Enable**: Available in any viewport view or diff.
- **Actions in Garage**:
  - Press `E` (`Action::Export`) or type `:export <filename>` in the command palette.
  - Enter the destination file path in the prompt and press `Enter`.

### 10.4. Full Mouse Interaction
- **Purpose & What You Can See**: Enables mouse interaction for scrolling, pane focusing, sidebar toggling, and def-use highlighting.
- **How to Enable**: Active automatically in terminals supporting mouse reporting.
- **Actions in Garage**:
  - **Scroll Wheel**: Scrolls the pane currently under the mouse pointer.
  - **Left Click on Pane**: Focuses the clicked pane (sidebar, viewport, or source pane) and positions the cursor.
  - **Left Click on Node Line**: Immediately activates semantic def-use highlighting for the clicked node.
  - **Left Click on Hidden Sidebar Strip (`▸`)**: Reopens the sidebar.
  - *(Note: Mouse capture overrides standard terminal text selection. To select text natively with your terminal, hold `Shift` while dragging, or hold `Option` in macOS Terminal.app. Alternatively, use `y`/`Y` for OSC 52 copying).*

### 10.5. Fully Customizable TOML Keymap (`--config`, `config.toml`)
- **Purpose & What You Can See**: Allows remapping any key binding or key chord in `garage`. A custom binding completely replaces the default chord (e.g. binding `quit = ["x"]` frees up `q`). All configurations are validated at startup before the alternate screen opens, reporting typos as clear error messages.
- **How to Enable**:
  - Create or edit `~/.config/garage/config.toml`, or specify a custom config path via `--config <path>`:
    ```sh
    garage --config ./my-keys.toml trace.log
    ```
  - Example TOML configuration:
    ```toml
    [keys]
    quit = ["x"]              # Replaces q with x
    half-page-down = "Ctrl+f"
    ```
- **Actions in Garage**:
  - Your custom keybindings take effect immediately upon startup.

### 10.6. Dynamic Keymap Help Modal (`?`)
- **Purpose & What You Can See**: Displays an interactive help modal listing all available actions and their current keybindings. The modal is dynamically generated from the live configuration table, ensuring that custom key remappings are accurately reflected.
- **How to Enable**: Available at all times.
- **Actions in Garage**:
  - Press `?` (`Action::Help`) to open the help modal.
  - Press `j` / `k` or `↑` / `↓` to scroll the help text.
  - Press `Esc`, `?`, or `q` to close the modal.

### 10.7. Command-Palette Quitting (`:q`, `:quit`)
- **Purpose & What You Can See**: Allows exiting `garage` from the command palette (`:q` or `:quit`), providing familiar vim-style quitting alongside the standard `q` and `Ctrl+c` keybindings.
- **How to Enable**: Available at all times.
- **Actions in Garage**:
  - Press `:`, type `q` or `quit`, and press `Enter` to quit `garage`.
