declare namespace InternalTTYBinding {
  interface TTYContext {
    code?: number;
    message?: string;
  }

  class TTY {
    constructor(fd: number, ctx: TTYContext);
  }
}

export interface TTYBinding {
  isTTY(fd: number): boolean;
  TTY: typeof InternalTTYBinding.TTY;
}
