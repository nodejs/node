"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Walker = void 0;
const path_1 = require("path");
const utils_1 = require("../utils");
const joinPath = __importStar(require("./functions/join-path"));
const pushDirectory = __importStar(require("./functions/push-directory"));
const pushFile = __importStar(require("./functions/push-file"));
const getArray = __importStar(require("./functions/get-array"));
const groupFiles = __importStar(require("./functions/group-files"));
const resolveSymlink = __importStar(require("./functions/resolve-symlink"));
const invokeCallback = __importStar(require("./functions/invoke-callback"));
const walkDirectory = __importStar(require("./functions/walk-directory"));
const queue_1 = require("./queue");
const counter_1 = require("./counter");
class Walker {
    root;
    isSynchronous;
    state;
    joinPath;
    pushDirectory;
    pushFile;
    getArray;
    groupFiles;
    resolveSymlink;
    walkDirectory;
    callbackInvoker;
    constructor(root, options, callback) {
        this.isSynchronous = !callback;
        this.callbackInvoker = invokeCallback.build(options, this.isSynchronous);
        this.root = (0, utils_1.normalizePath)(root, options);
        this.state = {
            root: (0, utils_1.isRootDirectory)(this.root) ? this.root : this.root.slice(0, -1),
            // Perf: we explicitly tell the compiler to optimize for String arrays
            paths: [""].slice(0, 0),
            groups: [],
            counts: new counter_1.Counter(),
            options,
            queue: new queue_1.Queue((error, state) => this.callbackInvoker(state, error, callback)),
            symlinks: new Map(),
            visited: [""].slice(0, 0),
        };
        /*
         * Perf: We conditionally change functions according to options. This gives a slight
         * performance boost. Since these functions are so small, they are automatically inlined
         * by the javascript engine so there's no function call overhead (in most cases).
         */
        this.joinPath = joinPath.build(this.root, options);
        this.pushDirectory = pushDirectory.build(this.root, options);
        this.pushFile = pushFile.build(options);
        this.getArray = getArray.build(options);
        this.groupFiles = groupFiles.build(options);
        this.resolveSymlink = resolveSymlink.build(options, this.isSynchronous);
        this.walkDirectory = walkDirectory.build(this.isSynchronous);
    }
    start() {
        this.walkDirectory(this.state, this.root, this.root, this.state.options.maxDepth, this.walk);
        return this.isSynchronous ? this.callbackInvoker(this.state, null) : null;
    }
    walk = (entries, directoryPath, depth) => {
        const { paths, options: { filters, resolveSymlinks, excludeSymlinks, exclude, maxFiles, signal, useRealPaths, pathSeparator, }, } = this.state;
        if ((signal && signal.aborted) || (maxFiles && paths.length > maxFiles))
            return;
        this.pushDirectory(directoryPath, paths, filters);
        const files = this.getArray(this.state.paths);
        for (let i = 0; i < entries.length; ++i) {
            const entry = entries[i];
            if (entry.isFile() ||
                (entry.isSymbolicLink() && !resolveSymlinks && !excludeSymlinks)) {
                const filename = this.joinPath(entry.name, directoryPath);
                this.pushFile(filename, files, this.state.counts, filters);
            }
            else if (entry.isDirectory()) {
                let path = joinPath.joinDirectoryPath(entry.name, directoryPath, this.state.options.pathSeparator);
                if (exclude && exclude(entry.name, path))
                    continue;
                this.walkDirectory(this.state, path, path, depth - 1, this.walk);
            }
            else if (entry.isSymbolicLink() && this.resolveSymlink) {
                let path = joinPath.joinPathWithBasePath(entry.name, directoryPath);
                this.resolveSymlink(path, this.state, (stat, resolvedPath) => {
                    if (stat.isDirectory()) {
                        resolvedPath = (0, utils_1.normalizePath)(resolvedPath, this.state.options);
                        if (exclude &&
                            exclude(entry.name, useRealPaths ? resolvedPath : path + pathSeparator))
                            return;
                        this.walkDirectory(this.state, resolvedPath, useRealPaths ? resolvedPath : path + pathSeparator, depth - 1, this.walk);
                    }
                    else {
                        resolvedPath = useRealPaths ? resolvedPath : path;
                        const filename = (0, path_1.basename)(resolvedPath);
                        const directoryPath = (0, utils_1.normalizePath)((0, path_1.dirname)(resolvedPath), this.state.options);
                        resolvedPath = this.joinPath(filename, directoryPath);
                        this.pushFile(resolvedPath, files, this.state.counts, filters);
                    }
                });
            }
        }
        this.groupFiles(this.state.groups, directoryPath, files);
    };
}
exports.Walker = Walker;
