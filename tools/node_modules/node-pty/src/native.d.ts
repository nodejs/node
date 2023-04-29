/**
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */

interface IConptyNative {
  startProcess(file: string, cols: number, rows: number, debug: boolean, pipeName: string, conptyInheritCursor: boolean): IConptyProcess;
  connect(ptyId: number, commandLine: string, cwd: string, env: string[], onExitCallback: (exitCode: number) => void): { pid: number };
  resize(ptyId: number, cols: number, rows: number): void;
  kill(ptyId: number): void;
}

interface IWinptyNative {
  startProcess(file: string, commandLine: string, env: string[], cwd: string, cols: number, rows: number, debug: boolean): IWinptyProcess;
  resize(processHandle: number, cols: number, rows: number): void;
  kill(pid: number, innerPidHandle: number): void;
  getProcessList(pid: number): number[];
  getExitCode(innerPidHandle: number): number;
}

interface IUnixNative {
  fork(file: string, args: string[], parsedEnv: string[], cwd: string, cols: number, rows: number, uid: number, gid: number, useUtf8: boolean, onExitCallback: (code: number, signal: number) => void): IUnixProcess;
  open(cols: number, rows: number): IUnixOpenProcess;
  process(fd: number, pty: string): string;
  resize(fd: number, cols: number, rows: number): void;
}

interface IConptyProcess {
  pty: number;
  fd: number;
  conin: string;
  conout: string;
}

interface IWinptyProcess {
  pty: number;
  fd: number;
  conin: string;
  conout: string;
  pid: number;
  innerPid: number;
  innerPidHandle: number;
}

interface IUnixProcess {
  fd: number;
  pid: number;
  pty: string;
}

interface IUnixOpenProcess {
  master: number;
  slave: number;
  pty: string;
}
