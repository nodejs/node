import { ChildProcessWithoutNullStreams, spawn } from 'child_process';
import { Subject } from 'rxjs';
import path from 'path';
import fs from 'fs';
import { Address, Handle, Loop, MIDINote, State, UVHandleTypeNames, UVRunModeName } from './types';

// max-api is not imported from node_modules, but from Node4Max runtime
// @ts-ignore - no typescript typings yet
import { post, outlet } from 'max-api';

const handles = new Map<Address, Handle>();
const loops = new Map<Address, Loop>();

const spawnNode = (...args: string[]): ChildProcessWithoutNullStreams => {
  // must compile node before running
  const nodePath = path.join(__dirname, '../../out/Release/node');
  return spawn(nodePath, args);
};

const parseLogData = (logLn: string): [State, Handle | Loop | undefined] | undefined => {
  const [name, ...data] = logLn.split(' ');
  switch (name.trim()) {
    case 'uv__handle_init':
      const [handleAddress, loopAddress, typeStr] = data;
      const type = Number.parseInt(typeStr)
      const handle: Handle = {
        type,
        typeName: UVHandleTypeNames[type],
        address: handleAddress,
        loop: loopAddress
      };
      handles.set(handleAddress, handle);
      return ['handle_init', handle];
    case 'uv__handle_start':
      const [startAddress] = data;
      return ['handle_start', handles.get(startAddress)];
    case 'uv__handle_stop':
      const [stopAddress] = data;
      return ['handle_stop', handles.get(stopAddress)];
    case 'uv_run':
      const [loopRunAddress,,modeStr] = data;
      const mode = Number.parseInt(modeStr)
      const loop: Loop = {
        type: mode + UVHandleTypeNames.length, // will continue the enumeration of handles, for note assignment
        typeName: UVRunModeName[mode],
        address: loopRunAddress
      };
      loops.set(loopRunAddress, loop);
      return ['loop_run', loop];
    case 'uv__loop_alive':
      const [loop_alive_address] = data;
      return ['loop_alive', loops.get(loop_alive_address)]
  }
}

export const createLogSubject = (proc: ChildProcessWithoutNullStreams): Subject<string> => {
  const log$ = new Subject<string>();

  proc.on('data', (chunk: Buffer) => chunk.toString().split('\n').forEach((str: string) => log$.next(str)));
  proc.stdout.on('data', (chunk: Buffer) => chunk.toString().split('\n').forEach((str: string) => log$.next(str)));
  proc.stdout.on('error', (err: Error) => log$.error(err));
  proc.on('error', (err: Error) => log$.error(err));
  proc.on('close', () => log$.complete());
  proc.on('exit', () => log$.complete());

  return log$;
};

try {
  // retrieve the third argument through destructuring
  // arguments are passed from a Max message
  const [,,scriptFile] = process.argv;

  const nodeProcess: ChildProcessWithoutNullStreams = spawnNode(scriptFile);
  createLogSubject(nodeProcess).subscribe((log: string) => {
    const [state, data] = parseLogData(log) ?? [];
    if (!data) return;

    const note: MIDINote = data.type + 100; // adds 100 for higher MIDI note range
    post(state, data.typeName, note);

    switch (state) {
      case 'handle_init':
        outlet([note, 50]);
        break;
      case 'handle_start':
        outlet([note, 100]);
        break;
      case 'handle_stop':
        outlet([note, 0]);
        break;
      case 'loop_run':
        outlet([note, 100]);
        break;
      case 'loop_alive':
        outlet([note, 50]);
        break;
    }
  });
} catch (err) {
  process.exit(1);
}
