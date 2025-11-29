import { resolve } from 'node:path';
import { spawn } from 'node:child_process';

export default class QuicTestClient {
  #pathToClient;
  #runningProcess;

  constructor() {
    this.#pathToClient = resolve(process.execPath, '../ngtcp2_test_client');
    console.log(this.#pathToClient);
  }

  help(options = { stdio: 'inherit' }) {
    const { promise, resolve, reject } = Promise.withResolvers();
    const proc = spawn(this.#pathToClient, ['--help'], options);
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

  run(address, port, uri, options = { stdio: 'inherit' }) {
    const { promise, resolve, reject } = Promise.withResolvers();
    if (this.#runningProcess) {
      reject(new Error('Server is already running'));
      return promise;
    }
    const args = [
      address,
      port,
      uri ?? '',
    ];
    this.#runningProcess = spawn(this.#pathToClient, args, options);
    this.#runningProcess.on('error', (err) => {
      this.#runningProcess = undefined;
      reject(err);
    });
    this.#runningProcess.on('exit', (code, signal) => {
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
