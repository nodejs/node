/**
 * Copyright (c) 2017, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */

import * as assert from 'assert';
import { WindowsTerminal } from './windowsTerminal';
import { UnixTerminal } from './unixTerminal';
import { Terminal } from './terminal';
import { Socket } from 'net';

const terminalConstructor = (process.platform === 'win32') ? WindowsTerminal : UnixTerminal;
const SHELL = (process.platform === 'win32') ? 'cmd.exe' : '/bin/bash';

let terminalCtor: WindowsTerminal | UnixTerminal;
if (process.platform === 'win32') {
  terminalCtor = require('./windowsTerminal');
} else {
  terminalCtor = require('./unixTerminal');
}

class TestTerminal extends Terminal {
  public checkType<T>(name: string, value: T, type: string, allowArray: boolean = false): void {
    this._checkType(name, value, type, allowArray);
  }
  protected _write(data: string): void {
    throw new Error('Method not implemented.');
  }
  public resize(cols: number, rows: number): void {
    throw new Error('Method not implemented.');
  }
  public destroy(): void {
    throw new Error('Method not implemented.');
  }
  public kill(signal?: string): void {
    throw new Error('Method not implemented.');
  }
  public get process(): string {
    throw new Error('Method not implemented.');
  }
  public get master(): Socket {
    throw new Error('Method not implemented.');
  }
  public get slave(): Socket {
    throw new Error('Method not implemented.');
  }
}

describe('Terminal', () => {
  describe('constructor', () => {
    it('should do basic type checks', () => {
      assert.throws(
        () => new (<any>terminalCtor)('a', 'b', { 'name': {} }),
        'name must be a string (not a object)'
      );
    });
  });

  describe('checkType', () => {
    it('should throw for the wrong type', () => {
      const t = new TestTerminal();
      assert.doesNotThrow(() => t.checkType('foo', 'test', 'string'));
      assert.doesNotThrow(() => t.checkType('foo', 1, 'number'));
      assert.doesNotThrow(() => t.checkType('foo', {}, 'object'));

      assert.throws(() => t.checkType('foo', 'test', 'number'));
      assert.throws(() => t.checkType('foo', 1, 'object'));
      assert.throws(() => t.checkType('foo', {}, 'string'));
    });
    it('should throw for wrong types within arrays', () => {
      const t = new TestTerminal();
      assert.doesNotThrow(() => t.checkType('foo', ['test'], 'string', true));
      assert.doesNotThrow(() => t.checkType('foo', [1], 'number', true));
      assert.doesNotThrow(() => t.checkType('foo', [{}], 'object', true));

      assert.throws(() => t.checkType('foo', ['test'], 'number', true));
      assert.throws(() => t.checkType('foo', [1], 'object', true));
      assert.throws(() => t.checkType('foo', [{}], 'string', true));
    });
  });

  describe('automatic flow control', () => {
    it('should respect ctor flow control options', () => {
      const pty = new terminalConstructor(SHELL, [], {handleFlowControl: true, flowControlPause: 'abc', flowControlResume: '123'});
      assert.equal(pty.handleFlowControl, true);
      assert.equal((pty as any)._flowControlPause, 'abc');
      assert.equal((pty as any)._flowControlResume, '123');
    });
    // TODO: I don't think this test ever worked due to pollUntil being used incorrectly
    // it('should do flow control automatically', async function(): Promise<void> {
    //   // Flow control doesn't work on Windows
    //   if (process.platform === 'win32') {
    //     return;
    //   }

    //   this.timeout(10000);
    //   const pty = new terminalConstructor(SHELL, [], {handleFlowControl: true, flowControlPause: 'PAUSE', flowControlResume: 'RESUME'});
    //   let read: string = '';
    //   pty.on('data', data => read += data);
    //   pty.on('pause', () => read += 'paused');
    //   pty.on('resume', () => read += 'resumed');
    //   pty.write('1');
    //   pty.write('PAUSE');
    //   pty.write('2');
    //   pty.write('RESUME');
    //   pty.write('3');
    //   await pollUntil(() => {
    //     return stripEscapeSequences(read).endsWith('1pausedresumed23');
    //   }, 100, 10);
    // });
  });
});

function stripEscapeSequences(data: string): string {
  return data.replace(/\u001b\[0K/, '');
}
