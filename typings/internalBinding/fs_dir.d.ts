import {InternalFSBinding, ReadFileContext} from './fs';

declare namespace InternalFSDirBinding {
  import FSReqCallback = InternalFSBinding.FSReqCallback;
  type Buffer = Uint8Array;
  type StringOrBuffer = string | Buffer;

  class DirHandle {
    read(encoding: string, bufferSize: number, _: unknown, ctx: ReadFileContext): string[] | undefined;
    close(_: unknown, ctx: ReadFileContext): void;
  }

  function opendir(path: StringOrBuffer, encoding: string, req: FSReqCallback): DirHandle;
  function opendir(path: StringOrBuffer, encoding: string, _: undefined, ctx: ReadFileContext): DirHandle;
  function opendirSync(path: StringOrBuffer): DirHandle;
}

export interface FsDirBinding {
  opendir: typeof InternalFSDirBinding.opendir;
  opendirSync: typeof InternalFSDirBinding.opendirSync;
}
