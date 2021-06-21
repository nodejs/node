export type State = 'handle_init'
  | 'handle_start'
  | 'handle_stop'
  | 'loop_run'
  | 'loop_alive';

export const UVHandleTypeNames = [
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
];

export enum UVHandleType {
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

export const UVRunModeName = [
  'UV_RUN_DEFAULT',
  'UV_RUN_ONCE',
  'UV_RUN_NOWAIT'
];

export enum UVRunMode {
  UV_RUN_DEFAULT = 19,
  UV_RUN_ONCE,
  UV_RUN_NOWAIT
}

export type Address = string;

export interface Handle {
  type: UVHandleType;
  typeName: string;
  address: Address;
  loop: Address;
}

export interface Loop {
  type: UVRunMode;
  typeName: string;
  address: Address
}

export type MIDINote = number;
