// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file out/extension.js gets automatically created from
// src/extension.ts. out/extension.js should not be modified manually.

import * as path from "path";
import { ExtensionContext, OutputChannel, StatusBarAlignment,
    window, workspace, WorkspaceConfiguration } from "vscode";
import { CloseAction, ErrorAction, ErrorHandler, Message,
    RevealOutputChannelOn, State, Trace } from "vscode-languageclient";

import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
} from "vscode-languageclient";

let client: LanguageClient;
let outputChannel: OutputChannel;

class TorqueErrorHandler implements ErrorHandler {
  constructor(readonly config: WorkspaceConfiguration) {}

  public error(error: Error, message: Message, count: number): ErrorAction {
    outputChannel.appendLine("TorqueErrorHandler: ");
    outputChannel.append(error.toString());
    outputChannel.append(message.toString());
    return ErrorAction.Continue;
  }

  public closed(): CloseAction {
    return CloseAction.DoNotRestart;
  }
}

export async function activate(context: ExtensionContext) {
  // Create a status bar item that displays the current status of the language server.
  const statusBarItem = window.createStatusBarItem(StatusBarAlignment.Left, 0);
  statusBarItem.text = "torque-ls: <unknown>";
  statusBarItem.show();

  const torqueConfiguration = workspace.getConfiguration("torque.ls");
  let serverExecutable: string | null = torqueConfiguration.get("executable");
  if (serverExecutable == null) {
    serverExecutable = path.join(workspace.rootPath, "out", "x64.release", "torque-language-server");
  }

  let serverArguments = [];
  const loggingEnabled: boolean = torqueConfiguration.get("logging");
  if (loggingEnabled) {
    const logfile = torqueConfiguration.get("logfile");
    serverArguments = ["-l", logfile];
  }

  const serverOptions: ServerOptions = { command: serverExecutable, args: serverArguments };

  outputChannel = window.createOutputChannel("Torque Language Server");

  const clientOptions: LanguageClientOptions = {
    diagnosticCollectionName: "torque",
    documentSelector: [{ scheme: "file", language: "torque" }],
    errorHandler: new TorqueErrorHandler(workspace.getConfiguration("torque")),
    initializationFailedHandler: (e) => {
      outputChannel.appendLine(e);
      return false;
    },
    outputChannel,
    revealOutputChannelOn: RevealOutputChannelOn.Info,
  };

  // Create the language client and start the client.
  client = new LanguageClient("torque", "Torque Language Server", serverOptions, clientOptions);
  client.trace = Trace.Verbose;

  // Update the status bar according to the client state.
  client.onDidChangeState((event) => {
    if (event.newState === State.Running) {
      statusBarItem.text = "torque-ls: Running";
    } else if (event.newState === State.Starting) {
      statusBarItem.text = "torque-ls: Starting";
    } else {
      statusBarItem.text = "torque-ls: Stopped";
    }
  });

  // This will start client and server.
  client.start();

  await client.onReady();

  // The server needs an initial list of all the Torque files
  // in the workspace, send them over.
  workspace.findFiles("**/*.tq").then((urls) => {
    client.sendNotification("torque/fileList",
      { files: urls.map((url) => url.toString())});
  });
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) { return undefined; }
  return client.stop();
}
