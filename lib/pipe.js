'use strict';

const { Socket } = require('net');

const {
  kReaderOfPair,
  kWriterOfPair,
} = require('internal/pipe');

const {
  Pipe,
  pairPipes,
  constants: PipeConstants,
} = internalBinding('pipe_wrap');

function createPipe() {
  const readHandle = new Pipe(PipeConstants.SOCKET);
  const writeHandle = new Pipe(PipeConstants.SOCKET);
  pairPipes(readHandle, writeHandle);

  const readable = new Socket({
    handle: readHandle,
    pauseOnCreate: true,
    readable: true,
    writable: false,
  });
  const writable = new Socket({
    handle: writeHandle,
    readable: false,
    writable: true,
  });

  readable[kReaderOfPair] = true;
  writable[kWriterOfPair] = true;

  return { readable, writable };
}

module.exports = {
  createPipe,
};
