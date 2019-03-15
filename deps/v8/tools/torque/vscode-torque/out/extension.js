"use strict";
// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
// The file out/extension.js gets automatically created from
// src/extension.ts. out/extension.js should not be modified manually.
const path = require("path");
const vscode_1 = require("vscode");
const vscode_languageclient_1 = require("vscode-languageclient");
const vscode_languageclient_2 = require("vscode-languageclient");
let client;
let outputChannel;
class TorqueErrorHandler {
    constructor(config) {
        this.config = config;
    }
    error(error, message, count) {
        outputChannel.appendLine("TorqueErrorHandler: ");
        outputChannel.append(error.toString());
        outputChannel.append(message.toString());
        return vscode_languageclient_1.ErrorAction.Continue;
    }
    closed() {
        return vscode_languageclient_1.CloseAction.DoNotRestart;
    }
}
function activate(context) {
    return __awaiter(this, void 0, void 0, function* () {
        // Create a status bar item that displays the current status of the language server.
        const statusBarItem = vscode_1.window.createStatusBarItem(vscode_1.StatusBarAlignment.Left, 0);
        statusBarItem.text = "torque-ls: <unknown>";
        statusBarItem.show();
        const torqueConfiguration = vscode_1.workspace.getConfiguration("torque.ls");
        let serverExecutable = torqueConfiguration.get("executable");
        if (serverExecutable == null) {
            serverExecutable = path.join(vscode_1.workspace.rootPath, "out", "x64.release", "torque-language-server");
        }
        let serverArguments = [];
        const loggingEnabled = torqueConfiguration.get("logging");
        if (loggingEnabled) {
            const logfile = torqueConfiguration.get("logfile");
            serverArguments = ["-l", logfile];
        }
        const serverOptions = { command: serverExecutable, args: serverArguments };
        outputChannel = vscode_1.window.createOutputChannel("Torque Language Server");
        const clientOptions = {
            diagnosticCollectionName: "torque",
            documentSelector: [{ scheme: "file", language: "torque" }],
            errorHandler: new TorqueErrorHandler(vscode_1.workspace.getConfiguration("torque")),
            initializationFailedHandler: (e) => {
                outputChannel.appendLine(e);
                return false;
            },
            outputChannel,
            revealOutputChannelOn: vscode_languageclient_1.RevealOutputChannelOn.Info,
        };
        // Create the language client and start the client.
        client = new vscode_languageclient_2.LanguageClient("torque", "Torque Language Server", serverOptions, clientOptions);
        client.trace = vscode_languageclient_1.Trace.Verbose;
        // Update the status bar according to the client state.
        client.onDidChangeState((event) => {
            if (event.newState === vscode_languageclient_1.State.Running) {
                statusBarItem.text = "torque-ls: Running";
            }
            else if (event.newState === vscode_languageclient_1.State.Starting) {
                statusBarItem.text = "torque-ls: Starting";
            }
            else {
                statusBarItem.text = "torque-ls: Stopped";
            }
        });
        // This will start client and server.
        client.start();
        yield client.onReady();
        // The server needs an initial list of all the Torque files
        // in the workspace, send them over.
        vscode_1.workspace.findFiles("**/*.tq").then((urls) => {
            client.sendNotification("torque/fileList", { files: urls.map((url) => url.toString()) });
        });
    });
}
exports.activate = activate;
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map