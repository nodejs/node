import { Worker, isMainThread, workerData, threadName } from 'worker_threads'
import fs from 'fs/promises'
import path from 'path'

// Number of iterations for each test
const n = 1000;

// Artificial delays to add in mock process
const mockDelayMs = 1;

async function main() {
  const { barrier, mockCWD, mockCWDCount } = await setup()
  if (isMainThread) {
    console.log("Node", process.version)
  }
  test("real process", barrier, process)
  test("mock process", barrier, fakeProcess(mockCWDCount, mockCWD))
  test("mock fixed process", barrier, fixedProcess(mockCWDCount, mockCWD))
}

function test(label: string, barrier: Barrier, testProcess: TestProcess) {
  let racesWon = 0;
  let problems = 0;

  for (let i = 0; i < n; i++) {
    const expected = dir(i)
    let first = ''

    barrier.wait()

    // The race!
    // main thread changes directory while tester checks what the directory is
    if (isMainThread) {
      testProcess.chdir(expected)
    } else if (threadName === 'tester') {
      first = testProcess.cwd()
    }

    barrier.wait()

    // We know the main thread has completed chdir so tester cwd should equal expected,
    // but we think there's a TOCTOU bug. If tester won the race and checked cwd simultaneously
    // with chdir then cwd will continue to return the stale value.
    if (threadName === 'tester') {
      const actual = testProcess.cwd()
      if (first != expected) {
        racesWon++
        if (actual == first) problems++
      } else {
        if (actual != expected) throw Error("Bug in tests")
      }
    }
  }

  //barrier.wait()

  if (threadName === 'tester') {
    console.log("Test case:", label)
    const wonPercent = (racesWon * 100 / n).toFixed(2)
    if (racesWon == 0) {
      console.log("  ⚠️The tester never called cwd before main thread chdir.")
      console.log("  Results are inconclusive, bad luck or bad test code.")
    } else if (problems > 0) {
      const problemPercent = (problems * 100 / racesWon).toFixed(2)
      console.log(`  ❌ cwd in worker thread reflected stale value`)
      console.log(`  errors: ${problemPercent}% ${problems}/${racesWon}`)
    } else {
      const percent = (racesWon * 100 / n).toFixed(2)
      console.log(`  ✅ cwd in worker thread always had expected value`)
    }
    console.log(`  races won: ${wonPercent}% ${racesWon}/${n}`)
  }
}

// Create directories to chdir to, start worker threads, and create shared resources.
async function setup() {
  const shraedMemSize = Barrier.BYTES_PER_ELEMENT * 1 + Int32Array.BYTES_PER_ELEMENT * 2
  const sharedBuffer = isMainThread ? new SharedArrayBuffer(shraedMemSize) : workerData.sharedBuffer;
  let off = 0;

  const barrier = new Barrier(sharedBuffer, off, 2);
  off += Barrier.BYTES_PER_ELEMENT;

  const mockCWDCount = new Int32Array(sharedBuffer, off, 1)
  off += mockCWDCount.byteLength

  const mockCWD = new Int32Array(sharedBuffer, off, 1)
  off += mockCWD.byteLength

  if (isMainThread) {
    // Create test directories
    for (let i = 0; i < n; i++) {
      await fs.mkdir(dir(i), { recursive: true })
    }

    // Spawn the worker
    new Worker(import.meta.filename, { workerData: { sharedBuffer }, name: 'coordinator' })
    new Worker(import.meta.filename, { workerData: { sharedBuffer }, name: 'tester' })
  }
  return { barrier, mockCWD, mockCWDCount }
}


// simulation of the real process
function fakeProcess(mockCWDCount: Int32Array, mockCWD: Int32Array) {
  let cachedCwd = '';
  let lastCounter = 0;

  return {
    chdir: (dir: string) => {
      const cwd = Number(path.basename(dir))

      // Increment counter, then store dir - like node's real chdir.
      busyWait(Math.random() * mockDelayMs)
      lastCounter = Atomics.add(mockCWDCount, 0, 1) + 1;
      busyWait(Math.random() * mockDelayMs)
      Atomics.store(mockCWD, 0, cwd);
    },
    cwd: () => {
      busyWait(Math.random() * mockDelayMs)
      const currentCounter = Atomics.load(mockCWDCount, 0);
      if (currentCounter === lastCounter) {
        return cachedCwd;
      }
      busyWait(Math.random() * mockDelayMs)
      lastCounter = currentCounter;
      cachedCwd = dir(Atomics.load(mockCWD, 0))
      return cachedCwd;
    }
  }
}

// same as fakeProcess but counter order changed, this should fix the bug
function fixedProcess(mockCWDCount: Int32Array, mockCWD: Int32Array) {
  let cachedCwd = '';
  let lastCounter = 0;

  return {
    chdir: (dir: string) => {
      const cwd = Number(path.basename(dir))

      // store dir, then increment counter which should fix the bug
      busyWait(Math.random() * mockDelayMs)
      Atomics.store(mockCWD, 0, cwd);
      busyWait(Math.random() * mockDelayMs)
      lastCounter = Atomics.add(mockCWDCount, 0, 1) + 1;
    },
    cwd: () => {
      busyWait(Math.random() * mockDelayMs)
      const currentCounter = Atomics.load(mockCWDCount, 0);
      if (currentCounter === lastCounter) {
        return cachedCwd;
      }
      busyWait(Math.random() * mockDelayMs)
      lastCounter = currentCounter;
      cachedCwd = dir(Atomics.load(mockCWD, 0))
      return cachedCwd;
    }
  }
}

const originalCwd = process.cwd()

// Return expected dir for Nth test
function dir(n: number): string {
  return `${originalCwd}/.tmp/${n}`
}

// block for a number of ms, including fractional sub-ms values.
// this is used in the mock processes so we don't have 100% races won or problems
// not necessary but it makes me more confident in the test/mock setup.
function busyWait(ms: number) {
  const end = performance.now() + ms;
  do {
    // @ts-ignore-error
    Atomics.pause()
  } while (performance.now() < end);
}

// class to synchronize threads
// this version relies on a 'coordinator' thread that releases
// the other threads, I think this makes the race more 'fair'
// as neither of the racers are holding the starting gun
// but you can reproduce the bug without a coordinator
class Barrier {
  static ELEMENTS = 2
  static BYTES_PER_ELEMENT = Int32Array.BYTES_PER_ELEMENT * Barrier.ELEMENTS

  count: number
  state: Int32Array
  constructor(shared: SharedArrayBuffer, off: number, count: number) {
    this.count = count;
    this.state = new Int32Array(shared, off, Barrier.ELEMENTS)
  }

  wait() {
    // index for number of waiting threads
    const WAITING = 0;
    // index for how many times this barrier has been used
    const GEN = 1;

    if (threadName === 'coordinator') {
      // the coordinator will reset waiting, notifying the threads
      // to resume
      while (true) {
        const waiting = Atomics.load(this.state, WAITING)
        if (waiting >= this.count) {
          Atomics.add(this.state, GEN, 1);
          Atomics.store(this.state, WAITING, 0);
          Atomics.notify(this.state, GEN);
          return;
        }
        Atomics.wait(this.state, WAITING, waiting)
      }
    } else {
      const counter = Atomics.load(this.state, GEN)
      Atomics.add(this.state, WAITING, 1);
      Atomics.notify(this.state, WAITING);
      Atomics.wait(this.state, GEN, counter);
    }
  }
}

type TestProcess = Pick<typeof process, 'chdir' | 'cwd'>

await main()
