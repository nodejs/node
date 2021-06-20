import { ChildProcessWithoutNullStreams, spawn } from 'child_process';
import { Subject } from 'rxjs';
import path from 'path';

// @ts-ignore
// import { post } from 'max-api';

const post = console.log;

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
  type: uv_handle_type,
  address: Address,
  loop: Address
}

enum State {
  init,
  start,
  stop,
  run,
  alive
}

interface Loop {
  address: Address
}

const handles = new Map<Address, Handle>();
const loops = new Map<Address, Loop>();

const spawnNode = (...args: string[]): ChildProcessWithoutNullStreams => {
  const nodePath = path.join(__dirname, '../../out/Release/node');
  post(`spawning node at ${nodePath}`);
  return spawn(nodePath, args);
};

const parseData = (logLn: string): [State, Handle | Loop | undefined] | undefined => {
  post(`parsing ${logLn}`);

  const [name, ...data] = logLn.split(' ');
  switch (name.trim()) {
    case 'uv__handle_init':
      const [handle_address, loop_address, type] = data;
      const handle: Handle = {
        type: Number.parseInt(type),
        address: handle_address,
        loop: loop_address
      };
      handles.set(handle_address, handle);
      return [State.init, handle];
    case 'uv__handle_start':
      const [start_address] = data;
      return [State.start, handles.get(start_address)];
    case 'uv__handle_stop':
      const [stop_address] = data;
      return [State.stop, handles.get(stop_address)];
    case 'uv_run':
      const [loop_run_address] = data;
      const loop: Loop = {
        address: loop_run_address
      };
      loops.set(loop_run_address, loop);
      return [State.run, loop];
    case 'uv__loop_alive':
      const [loop_alive_address] = data;
      return [State.alive, loops.get(loop_alive_address)]
  }
}

export const createLogger = (proc: ChildProcessWithoutNullStreams): Subject<string> => {
  post('creating logger');
  const logger$ = new Subject<string>();

  proc.on('data', (chunk: Buffer) => logger$.next(chunk.toString()));
  proc.stdout.on('data', (chunk: Buffer) => logger$.next(chunk.toString()));
  proc.stdout.on('error', (err: Error) => logger$.error(err));
  proc.on('error', (err: Error) => logger$.error(err));
  proc.on('close', () => logger$.complete());
  proc.on('exit', () => logger$.complete());

  return logger$;
};

try {
  post('will start');
  const proc: ChildProcessWithoutNullStreams = spawnNode('-e', '2+2');
  const logger$ = createLogger(proc);
  logger$.subscribe((log: string) => {
    const [state, handle] = parseData(log) ?? [];
    post(`${state} ${handle}`);
  });
} catch (err) {
  post(`${err}`);
  process.exit(1);
}
