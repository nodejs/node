import { ChildProcessWithoutNullStreams, spawn } from 'child_process';
import { Subject } from 'rxjs';
import path from 'path';

// @ts-ignore
import { post, outlet } from 'max-api';

// const post = console.log;
// const outlet = console.log;

type State = 'handle_init'
  | 'handle_start'
  | 'handle_stop'
  | 'loop_run'
  | 'loop_alive';

const uv_handle_type_names = [
  'UV_UNKNOWN_HANDLE',
  'UV_ASYNC',
  'UV_CHECK',
  'UV_FS_EVENT',
  'UV_FS_POLL',
  'UV_HANDLE',
  'UV_IDLE',
  'UV_NAMED_PIPE',
  'UV_POLL',
  'UV_PREPARE',
  'UV_PROCESS',
  'UV_STREAM',
  'UV_TCP',
  'UV_TIMER',
  'UV_TTY',
  'UV_UDP',
  'UV_SIGNAL',
  'UV_FILE',
  'UV_HANDLE_TYPE_MAX'
]

enum uv_handle_type {
  UV_UNKNOWN_HANDLE,
  UV_ASYNC,
  UV_CHECK,
  UV_FS_EVENT,
  UV_FS_POLL,
  UV_HANDLE,
  UV_IDLE,
  UV_NAMED_PIPE,
  UV_POLL,
  UV_PREPARE,
  UV_PROCESS,
  UV_STREAM,
  UV_TCP,
  UV_TIMER,
  UV_TTY,
  UV_UDP,
  UV_SIGNAL,
  UV_FILE,
  UV_HANDLE_TYPE_MAX
}

enum uv_run_mode {
  UV_RUN_DEFAULT,
  UV_RUN_ONCE,
  UV_RUN_NOWAIT
}

type Address = string;

interface Handle {
  type: uv_handle_type;
  typeName: string;
  address: Address;
  loop: Address;
}

interface Loop {
  address: Address
}

const handles = new Map<Address, Handle>();
const loops = new Map<Address, Loop>();

const spawnNode = (...args: string[]): ChildProcessWithoutNullStreams => {
  const nodePath = path.join(__dirname, '../../out/Release/node');
  return spawn(nodePath, args);
};

const parseData = (logLn: string): [State, Handle | Loop | undefined] | undefined => {
  const [name, ...data] = logLn.split(' ');
  switch (name.trim()) {
    case 'uv__handle_init':
      const [handle_address, loop_address, typeStr] = data;
      const type = Number.parseInt(typeStr)
      const handle: Handle = {
        type,
        typeName: uv_handle_type_names[type],
        address: handle_address,
        loop: loop_address
      };
      handles.set(handle_address, handle);
      return ['handle_init', handle];
    case 'uv__handle_start':
      const [start_address] = data;
      return ['handle_start', handles.get(start_address)];
    case 'uv__handle_stop':
      const [stop_address] = data;
      return ['handle_stop', handles.get(stop_address)];
    case 'uv_run':
      const [loop_run_address] = data;
      const loop: Loop = {
        address: loop_run_address
      };
      loops.set(loop_run_address, loop);
      return ['loop_run', loop];
    case 'uv__loop_alive':
      const [loop_alive_address] = data;
      return ['loop_alive', loops.get(loop_alive_address)]
  }
}

export const createLogger = (proc: ChildProcessWithoutNullStreams): Subject<string> => {
  const logger$ = new Subject<string>();
  const splitChunks = (chunk: Buffer) => chunk.toString().split('\n').forEach((str: string) => logger$.next(str))

  proc.on('data', splitChunks);
  proc.stdout.on('data', splitChunks);
  proc.stdout.on('error', (err: Error) => logger$.error(err));
  proc.on('error', (err: Error) => logger$.error(err));
  proc.on('close', () => logger$.complete());
  proc.on('exit', () => logger$.complete());

  return logger$;
};

try {
  const [,,scriptFile] = process.argv;

  const proc: ChildProcessWithoutNullStreams = spawnNode(scriptFile);
  const logger$ = createLogger(proc);
  logger$.subscribe((log: string) => {
    const [state, data] = parseData(log) ?? [];
    if (!data) return;

    // @ts-ignore
    const note = data.type + 100;
    // @ts-ignore
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
      // case 'loop_run':
      // case 'loop_alive':
    }
  });
} catch (err) {
  process.exit(1);
}
