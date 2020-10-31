// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

export = Wasm;

declare function Wasm(_: Wasm.ModuleArgs): Wasm.Module;

// See https://kripken.github.io/emscripten-site/docs/api_reference/module.html
declare namespace Wasm {
  export interface InitWasm {
    (_: ModuleArgs): Module;
  }

  export interface FileSystemType {}

  export interface FileSystemTypes {
    MEMFS: FileSystemType;
    IDBFS: FileSystemType;
    WORKERFS: FileSystemType;
  }

  export interface FileSystemNode {
    contents: Uint8Array;
    usedBytes: number;
  }

  export interface FileSystem {
    mkdir(path: string, mode?: number): any;
    mount(type: Wasm.FileSystemType, opts: any, mountpoint: string): any;
    unmount(mountpoint: string): void;
    unlink(mountpoint: string): void;
    lookupPath(path: string): {path: string, node: Wasm.FileSystemNode};
    filesystems: Wasm.FileSystemTypes;
  }

  export interface Module {
    callMain(args: string[]): void;
    addFunction(f: any, argTypes: string): void;
    ccall(
        ident: string,
        returnType: string,
        argTypes: string[],
        args: any[],
        ): number;
    HEAPU8: Uint8Array;
    FS: FileSystem;
  }

  export interface ModuleArgs {
    noInitialRun?: boolean;
    locateFile(s: string): string;
    print(s: string): void;
    printErr(s: string): void;
    onRuntimeInitialized(): void;
    onAbort(): void;
  }
}
