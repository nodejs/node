/**
 * Copyright (c) 2017, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */

import { UnixTerminal } from './unixTerminal';
import * as assert from 'assert';
import * as cp from 'child_process';
import * as path from 'path';
import { pollUntil } from './testUtils.test';

const FIXTURES_PATH = path.normalize(path.join(__dirname, '..', 'fixtures', 'utf8-character.txt'));

if (process.platform !== 'win32') {
  describe('UnixTerminal', () => {
    describe('Constructor', () => {
      it('should set a valid pts name', () => {
        const term = new UnixTerminal('/bin/bash', [], {});
        let regExp: RegExp;
        if (process.platform === 'linux') {
          // https://linux.die.net/man/4/pts
          regExp = /^\/dev\/pts\/\d+$/;
        }
        if (process.platform === 'darwin') {
          // https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man4/pty.4.html
          regExp = /^\/dev\/tty[p-sP-S][a-z0-9]+$/;
        }
        if (regExp) {
          assert.ok(regExp.test((<any>term)._pty), '"' + (<any>term)._pty + '" should match ' + regExp.toString());
        }
      });
    });

    describe('PtyForkEncodingOption', () => {
      it('should default to utf8', (done) => {
        const term = new UnixTerminal('/bin/bash', [ '-c', `cat "${FIXTURES_PATH}"` ]);
        term.on('data', (data) => {
          assert.equal(typeof data, 'string');
          assert.equal(data, '\u00E6');
          done();
        });
      });
      it('should return a Buffer when encoding is null', (done) => {
        const term = new UnixTerminal('/bin/bash', [ '-c', `cat "${FIXTURES_PATH}"` ], {
          encoding: null
        });
        term.on('data', (data) => {
          assert.equal(typeof data, 'object');
          assert.ok(data instanceof Buffer);
          assert.equal(0xC3, data[0]);
          assert.equal(0xA6, data[1]);
          done();
        });
      });
      it('should support other encodings', (done) => {
        const text = 'test æ!';
        const term = new UnixTerminal(null, ['-c', 'echo "' + text + '"'], {
          encoding: 'base64'
        });
        let buffer = '';
        term.on('data', (data) => {
          assert.equal(typeof data, 'string');
          buffer += data;
        });
        term.on('exit', () => {
          assert.equal(Buffer.alloc(8, buffer, 'base64').toString().replace('\r', '').replace('\n', ''), text);
          done();
        });
      });
    });

    describe('open', () => {
      let term: UnixTerminal;

      afterEach(() => {
        if (term) {
          term.slave.destroy();
          term.master.destroy();
        }
      });

      it('should open a pty with access to a master and slave socket', (done) => {
        term = UnixTerminal.open({});

        let slavebuf = '';
        term.slave.on('data', (data) => {
          slavebuf += data;
        });

        let masterbuf = '';
        term.master.on('data', (data) => {
          masterbuf += data;
        });

        pollUntil(() => {
          if (masterbuf === 'slave\r\nmaster\r\n' && slavebuf === 'master\n') {
            done();
            return true;
          }
          return false;
        }, 200, 10);

        term.slave.write('slave\n');
        term.master.write('master\n');
      });
    });
    describe('signals in parent and child', () => {
      it('SIGINT - custom in parent and child', done => {
        // this test is cumbersome - we have to run it in a sub process to
        // see behavior of SIGINT handlers
        const data = `
        var pty = require('./lib/index');
        process.on('SIGINT', () => console.log('SIGINT in parent'));
        var ptyProcess = pty.spawn('node', ['-e', 'process.on("SIGINT", ()=>console.log("SIGINT in child"));setTimeout(() => null, 300);'], {
          name: 'xterm-color',
          cols: 80,
          rows: 30,
          cwd: process.env.HOME,
          env: process.env
        });
        ptyProcess.on('data', function (data) {
          console.log(data);
        });
        setTimeout(() => null, 500);
        console.log('ready', ptyProcess.pid);
        `;
        const buffer: string[] = [];
        const p = cp.spawn('node', ['-e', data]);
        let sub = '';
        p.stdout.on('data', (data) => {
          if (!data.toString().indexOf('ready')) {
            sub = data.toString().split(' ')[1].slice(0, -1);
            setTimeout(() => {
              process.kill(parseInt(sub), 'SIGINT');  // SIGINT to child
              p.kill('SIGINT');                       // SIGINT to parent
            }, 200);
          } else {
            buffer.push(data.toString().replace(/^\s+|\s+$/g, ''));
          }
        });
        p.on('close', () => {
          // handlers in parent and child should have been triggered
          assert.equal(buffer.indexOf('SIGINT in child') !== -1, true);
          assert.equal(buffer.indexOf('SIGINT in parent') !== -1, true);
          done();
        });
      });
      it('SIGINT - custom in parent, default in child', done => {
        // this tests the original idea of the signal(...) change in pty.cc:
        // to make sure the SIGINT handler of a pty child is reset to default
        // and does not interfere with the handler in the parent
        const data = `
        var pty = require('./lib/index');
        process.on('SIGINT', () => console.log('SIGINT in parent'));
        var ptyProcess = pty.spawn('node', ['-e', 'setTimeout(() => console.log("should not be printed"), 300);'], {
          name: 'xterm-color',
          cols: 80,
          rows: 30,
          cwd: process.env.HOME,
          env: process.env
        });
        ptyProcess.on('data', function (data) {
          console.log(data);
        });
        setTimeout(() => null, 500);
        console.log('ready', ptyProcess.pid);
        `;
        const buffer: string[] = [];
        const p = cp.spawn('node', ['-e', data]);
        let sub = '';
        p.stdout.on('data', (data) => {
          if (!data.toString().indexOf('ready')) {
            sub = data.toString().split(' ')[1].slice(0, -1);
            setTimeout(() => {
              process.kill(parseInt(sub), 'SIGINT');  // SIGINT to child
              p.kill('SIGINT');                       // SIGINT to parent
            }, 200);
          } else {
            buffer.push(data.toString().replace(/^\s+|\s+$/g, ''));
          }
        });
        p.on('close', () => {
          // handlers in parent and child should have been triggered
          assert.equal(buffer.indexOf('should not be printed') !== -1, false);
          assert.equal(buffer.indexOf('SIGINT in parent') !== -1, true);
          done();
        });
      });
      it('SIGHUP default (child only)', done => {
        const term = new UnixTerminal('node', [ '-e', `
        console.log('ready');
        setTimeout(()=>console.log('timeout'), 200);`
        ]);
        let buffer = '';
        term.on('data', (data) => {
          if (data === 'ready\r\n') {
            term.kill();
          } else {
            buffer += data;
          }
        });
        term.on('exit', () => {
          // no timeout in buffer
          assert.equal(buffer, '');
          done();
        });
      });
      it('SIGUSR1 - custom in parent and child', done => {
        let pHandlerCalled = 0;
        const handleSigUsr = function(h: any): any {
          return function(): void {
            pHandlerCalled += 1;
            process.removeListener('SIGUSR1', h);
          };
        };
        process.on('SIGUSR1', handleSigUsr(handleSigUsr));

        const term = new UnixTerminal('node', [ '-e', `
        process.on('SIGUSR1', () => {
          console.log('SIGUSR1 in child');
        });
        console.log('ready');
        setTimeout(()=>null, 200);`
        ]);
        let buffer = '';
        term.on('data', (data) => {
          if (data === 'ready\r\n') {
            process.kill(process.pid, 'SIGUSR1');
            term.kill('SIGUSR1');
          } else {
            buffer += data;
          }
        });
        term.on('exit', () => {
          // should have called both handlers and only once
          assert.equal(pHandlerCalled, 1);
          assert.equal(buffer, 'SIGUSR1 in child\r\n');
          done();
        });
      });
    });
  });
}
