/**
 * Copyright (c) 2017, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */

export type ArgvOrCommandLine = string[] | string;

export interface IExitEvent {
  exitCode: number;
  signal: number | undefined;
}

export interface IDisposable {
  dispose(): void;
}
