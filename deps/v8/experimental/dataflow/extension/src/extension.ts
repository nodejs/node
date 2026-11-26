// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as vscode from 'vscode';
import * as child_process from 'child_process';
import * as readline from 'readline';
import * as path from 'path';
import * as fs from 'fs';

// DFG Node representation in TypeScript (matches C++ Node)
interface DfgNode {
    id: string;
    kind: string;
    name: string;
    type: string;
    location: {
        file: string;
        line: number;
        column: number;
    };
}

// DFG Query Result representation
interface QueryResultNode {
    node: DfgNode | null;
    flow_kind: string;
    flow_location: {
        file: string;
        line: number;
        column: number;
    };
    children: QueryResultNode[];
    is_cycle?: boolean;
}

class DfgClient {
    private process: child_process.ChildProcess | null = null;
    private rl: readline.Interface | null = null;
    private requestCounter = 0;
    private pendingRequests = new Map<number, { resolve: (val: any) => void, reject: (err: any) => void }>();
    private progressListener: ((progress: { title: string, message: string, percentage: number }) => void) | null = null;

    constructor(private binaryPath: string, private outputChannel: vscode.OutputChannel) {}

    onProgress(listener: (progress: { title: string, message: string, percentage: number }) => void) {
        this.progressListener = listener;
    }

    start(): Promise<void> {
        return new Promise((resolve, reject) => {
            this.outputChannel.appendLine(`Spawning backend: ${this.binaryPath} --server`);
            this.process = child_process.spawn(this.binaryPath, ['--server']);

            this.process.on('error', (err) => {
                this.outputChannel.appendLine(`Failed to start backend process: ${err.message}`);
                reject(err);
            });

            this.process.stderr?.on('data', (data) => {
                this.outputChannel.append(`[Backend Log] ${data}`);
            });

            this.rl = readline.createInterface({
                input: this.process.stdout!,
                output: this.process.stdin!
            });

            this.rl.on('line', (line) => {
                try {
                    const response = JSON.parse(line);
                    if (response.method === 'window/progress' && this.progressListener) {
                        this.progressListener(response.params);
                        return;
                    }
                    const id = response.id;
                    const pending = this.pendingRequests.get(id);
                    if (pending) {
                        this.pendingRequests.delete(id);
                        if (response.error) {
                            pending.reject(response.error);
                        } else {
                            pending.resolve(response.result);
                        }
                    }
                } catch (e) {
                    this.outputChannel.appendLine(`Failed to parse response line: ${line}. Error: ${e}`);
                }
            });

            // Small delay to ensure process is up
            setTimeout(() => {
                if (this.process && this.process.pid) {
                    resolve();
                } else {
                    reject(new Error('Backend failed to start.'));
                }
            }, 500);
        });
    }

    sendRequest(method: string, params: any): Promise<any> {
        return new Promise((resolve, reject) => {
            if (!this.process || !this.process.stdin) {
                return reject(new Error('Server not running'));
            }
            const id = ++this.requestCounter;
            const request = {
                jsonrpc: '2.0',
                id,
                method,
                params
            };
            this.pendingRequests.set(id, { resolve, reject });
            this.process.stdin.write(JSON.stringify(request) + '\n');
        });
    }

    stop() {
        if (this.process) {
            this.process.kill();
            this.process = null;
        }
        if (this.rl) {
            this.rl.close();
            this.rl = null;
        }
    }
}

// Tree Item representing a node in the data flow tree
class FlowTreeItem extends vscode.TreeItem {
    constructor(
        public readonly resultNode: QueryResultNode,
        public readonly collapsibleState: vscode.TreeItemCollapsibleState
    ) {
        let nodeName = resultNode.node ? resultNode.node.name : 'Unknown';
        const nodeType = resultNode.node ? resultNode.node.type : '';
        
        if (resultNode.is_cycle) {
            nodeName = `[Cycle] ${nodeName}`;
        }
        super(`${nodeName} : ${nodeType}`, collapsibleState);

        const flowKind = resultNode.flow_kind;
        let loc = resultNode.node ? resultNode.node.location : null;
        
        // Prioritize assignment/flow location over declaration location!
        if (resultNode.flow_location && resultNode.flow_location.file && resultNode.flow_location.line > 0) {
            loc = resultNode.flow_location;
        }
        
        if (loc) {
            const fileName = path.basename(loc.file);
            this.description = `${fileName}:${loc.line}:${loc.column} (${flowKind})`;
        } else {
            this.description = `(${flowKind})`;
        }

        // Add tooltip with full path
        if (loc) {
            this.tooltip = `${loc.file}:${loc.line}:${loc.column}\nFlow Kind: ${flowKind}`;
        }

        // Context value for context menu actions if needed
        this.contextValue = 'flowNode';

        // Define command to jump to source code when clicked
        if (loc) {
            this.command = {
                command: 'vscode.open',
                title: 'Open Source Location',
                arguments: [
                    vscode.Uri.file(loc.file),
                    {
                        selection: new vscode.Range(
                            loc.line - 1,
                            loc.column - 1,
                            loc.line - 1,
                            loc.column - 1
                        )
                    }
                ]
            };
        }
    }
}

class FlowTreeDataProvider implements vscode.TreeDataProvider<QueryResultNode> {
    private _onDidChangeTreeData: vscode.EventEmitter<QueryResultNode | undefined | null | void> = new vscode.EventEmitter<QueryResultNode | undefined | null | void>();
    readonly onDidChangeTreeData: vscode.Event<QueryResultNode | undefined | null | void> = this._onDidChangeTreeData.event;

    private root: QueryResultNode | null = null;

    setRoot(root: QueryResultNode | null) {
        this.root = root;
        this._onDidChangeTreeData.fire();
    }

    getTreeItem(element: QueryResultNode): vscode.TreeItem {
        return new FlowTreeItem(
            element,
            element.children.length > 0
                ? vscode.TreeItemCollapsibleState.Expanded
                : vscode.TreeItemCollapsibleState.None
        );
    }

    getChildren(element?: QueryResultNode): vscode.ProviderResult<QueryResultNode[]> {
        if (!element) {
            return this.root ? [this.root] : [];
        }
        return element.children;
    }
}

export async function activate(context: vscode.ExtensionContext) {
    const outputChannel = vscode.window.createOutputChannel("C++ Data Flow");
    outputChannel.appendLine('C++ Data Flow Analyzer Extension activated.');

    const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    if (!workspaceRoot) {
        outputChannel.appendLine('No workspace folder open. Extension disabled.');
        return;
    }

    // Find backend binary
    // 1. Check configuration
    // 2. Check default locations: workspaceRoot/build/analyzer
    let binaryPath = vscode.workspace.getConfiguration('cppDataFlow').get<string>('analyzerPath');
    if (!binaryPath) {
        binaryPath = path.join(workspaceRoot, 'build', 'analyzer');
    }

    if (!fs.existsSync(binaryPath)) {
        // Fallback to check if it's in bin or just raw 'analyzer' if in path
        const binPath = path.join(workspaceRoot, 'bin', 'analyzer');
        if (fs.existsSync(binPath)) {
            binaryPath = binPath;
        } else {
            vscode.window.showWarningMessage(`C++ Data Flow Analyzer binary not found at ${binaryPath}. Please configure the path in settings.`);
            // Keep binaryPath as is, maybe it's in PATH
        }
    }

    const client = new DfgClient(binaryPath, outputChannel);
    const treeDataProvider = new FlowTreeDataProvider();
    const treeView = vscode.window.createTreeView('vscode-dataflow.views.flowTree', {
        treeDataProvider
    });

    context.subscriptions.push(treeView);

    try {
        await client.start();
        outputChannel.appendLine('Backend server connected.');

        // Initialize server
        const cacheDir = path.join(workspaceRoot, '.cache');
        const compileCommandsDir = path.join(workspaceRoot, 'build');

        vscode.window.withProgress({
            location: vscode.ProgressLocation.Notification,
            title: "C++ Data Flow: Building Index",
            cancellable: false
        }, async (progress) => {
            let lastPercentage = 0;
            client.onProgress((p) => {
                const increment = p.percentage - lastPercentage;
                lastPercentage = p.percentage;
                progress.report({
                    message: p.message,
                    increment: increment
                });
            });

            try {
                const concurrency = vscode.workspace.getConfiguration('cppDataFlow').get<number>('concurrency') || 0;
                const initResult = await client.sendRequest('initialize', {
                    workspaceRoot,
                    cacheDir,
                    compileCommandsDir,
                    concurrency
                });
                outputChannel.appendLine(`Server initialized: ${JSON.stringify(initResult)}`);
                vscode.window.showInformationMessage(`C++ Data Flow Index built: ${initResult.nodesCount} nodes, ${initResult.edgesCount} edges.`);
            } catch (err: any) {
                outputChannel.appendLine(`Initialization failed: ${err.message || err}`);
                vscode.window.showErrorMessage(`Failed to initialize DFG server: ${err.message || err}`);
            }
        });

    } catch (err: any) {
        outputChannel.appendLine(`Failed to start backend: ${err.message}`);
        vscode.window.showErrorMessage(`Failed to start C++ Data Flow backend: ${err.message}`);
        return;
    }

    let lastQueryFile = "";
    let lastQueryLine = 0;
    let lastQueryColumn = 0;

    const runQuery = async (method: string, overrideFile?: string, overrideLine?: number, overrideColumn?: number) => {
        let file = overrideFile;
        let line = overrideLine;
        let column = overrideColumn;

        if (!file || !line || !column) {
            const editor = vscode.window.activeTextEditor;
            if (!editor) {
                return;
            }
            const document = editor.document;
            const position = editor.selection.active;
            file = document.uri.fsPath;
            line = position.line + 1;
            column = position.character + 1;
        }

        // Cache coordinates for toggle refreshing!
        lastQueryFile = file;
        lastQueryLine = line;
        lastQueryColumn = column;

        const effectiveMethod =
            (method === 'query/incoming' && isFlipped) ? 'query/from-leaves' : method;

        let title = "Analyzing Flow...";
        if (effectiveMethod === 'query/incoming') {
            title = "Analyzing Incoming Flow...";
        } else if (effectiveMethod === 'query/outgoing') {
            title = "Analyzing Outgoing Flow...";
        } else if (effectiveMethod === 'query/from-leaves') {
            title = "Tracing Forward from Leaves...";
        }

        vscode.window.withProgress({
            location: vscode.ProgressLocation.Window,
            title: title
        }, async () => {
            try {
                outputChannel.appendLine(`Running query ${effectiveMethod} at ${file}:${line}:${column}`);
                const result = await client.sendRequest(effectiveMethod, {
                    file,
                    line,
                    column
                });

                if (result) {
                    let finalTree = result;
                    if (effectiveMethod === 'query/incoming' && pruneCycles) {
                        finalTree = pruneIncomingCycles(result) || result;
                    }
                    treeDataProvider.setRoot(finalTree);
                    // Focus the tree view
                    treeView.reveal(finalTree, { select: true, focus: true, expand: true });
                } else {
                    treeDataProvider.setRoot(null);
                    vscode.window.showInformationMessage('No data flow found at this location.');
                }
            } catch (err: any) {
                outputChannel.appendLine(`Query failed: ${err.message || err}`);
                vscode.window.showErrorMessage(`Query failed: ${err.message || err}`);
            }
        });
    };

    context.subscriptions.push(
        vscode.commands.registerCommand('vscode-dataflow.analyzeIncoming', () => {
            runQuery('query/incoming');
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('vscode-dataflow.analyzeOutgoing', () => {
            runQuery('query/outgoing');
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('vscode-dataflow.analyzeFromLeaves', () => {
            isFlipped = true;
            vscode.commands.executeCommand('setContext', 'cppDataFlow.isFlipped', isFlipped);
            runQuery('query/from-leaves');
        })
    );

    let isFlipped = false;
    let pruneCycles = true;
    vscode.commands.executeCommand('setContext', 'cppDataFlow.isFlipped', isFlipped);
    vscode.commands.executeCommand('setContext', 'cppDataFlow.pruneCycles', pruneCycles);

    context.subscriptions.push(
        vscode.commands.registerCommand('vscode-dataflow.toggleFlowDirection', () => {
            isFlipped = !isFlipped;
            vscode.commands.executeCommand('setContext', 'cppDataFlow.isFlipped', isFlipped);
            
            if (lastQueryFile && lastQueryLine > 0) {
                runQuery(isFlipped ? 'query/from-leaves' : 'query/incoming', lastQueryFile, lastQueryLine, lastQueryColumn);
            } else {
                vscode.window.showInformationMessage(isFlipped ? "Direction set to: Flipped (Leaves -> Cursor)" : "Direction set to: Normal (Cursor -> Leaves)");
            }
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('vscode-dataflow.toggleCyclePruning', () => {
            pruneCycles = !pruneCycles;
            vscode.commands.executeCommand('setContext', 'cppDataFlow.pruneCycles', pruneCycles);
            
            if (lastQueryFile && lastQueryLine > 0) {
                runQuery(isFlipped ? 'query/from-leaves' : 'query/incoming', lastQueryFile, lastQueryLine, lastQueryColumn);
            } else {
                vscode.window.showInformationMessage(pruneCycles ? "Cycle Pruning: ON (Hidden)" : "Cycle Pruning: OFF (Visible)");
            }
        })
    );

    context.subscriptions.push({
        dispose: () => {
            client.stop();
            outputChannel.appendLine('C++ Data Flow Analyzer backend stopped.');
        }
    });
}

// Clean, recursive in-memory cycle pruning algorithm in TypeScript
function pruneIncomingCycles(node: QueryResultNode): QueryResultNode | null {
    if (node.is_cycle) {
        return null; // Pruned!
    }
    
    const newNode: QueryResultNode = {
        node: node.node ? { ...node.node } : null,
        flow_kind: node.flow_kind,
        flow_location: { ...node.flow_location },
        children: [],
        is_cycle: node.is_cycle
    };
    
    for (const child of node.children) {
        const prunedChild = pruneIncomingCycles(child);
        if (prunedChild) {
            newNode.children.push(prunedChild);
        }
    }
    
    return newNode;
}

export function deactivate() {}
