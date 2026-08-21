import { resolve } from 'node:path';
import { spawn } from 'node:child_process';

export default class QuicTestServer {
  #pathToServer;
  #runningProcess;

  constructor() {
    this.#pathToServer = resolve(process.execPath, '../ngtcp2_test_server');
    console.log(this.#pathToServer);
  }

  help(options = { stdio: 'inherit' }) {
    const { promise, resolve, reject } = Promise.withResolvers();
    const proc = spawn(this.#pathToServer, ['--help'], options);
    proc.on('error', reject);
    proc.on('exit', (code, signal) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`Process exited with code ${code} and signal ${signal}`));
      }
    });
    return promise;
  }

  run(address, port, keyFile, certFile, options = { stdio: 'inherit' }) {
    const { promise, resolve, reject } = Promise.withResolvers();
    if (this.#runningProcess) {
      reject(new Error('Server is already running'));
      return promise;
    }
    const args = [
      address,
      port,
      keyFile,
      certFile,
    ];
    this.#runningProcess = spawn(this.#pathToServer, args, options);
    this.#runningProcess.on('error', (err) => {
      this.#runningProcess = undefined;
      reject(err);
    });
    this.#runningProcess.on('exit', (code, signal) => {
      this.#runningProcess = undefined;
      if (code === 0) {
        resolve();
      } else {
        if (code === null && signal === 'SIGTERM') {
          // Normal termination due to stop() being called.
          resolve();
          return;
        }
        reject(new Error(`Process exited with code ${code} and signal ${signal}`));
      }
    });
    return promise;
  }

  stop() {
    if (this.#runningProcess) {
      this.#runningProcess.kill();
      this.#runningProcess = undefined;
    }
  }
};
