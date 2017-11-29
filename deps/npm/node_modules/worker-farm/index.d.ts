interface Workers {
  (callback: WorkerCallback): void;
  (arg1: any, callback: WorkerCallback): void;
  (arg1: any, arg2: any, callback: WorkerCallback): void;
  (arg1: any, arg2: any, arg3: any, callback: WorkerCallback): void;
  (arg1: any, arg2: any, arg3: any, arg4: any, callback: WorkerCallback): void;
}

type WorkerCallback =
  | WorkerCallback0
  | WorkerCallback1
  | WorkerCallback2
  | WorkerCallback3
  | WorkerCallback4;

type WorkerCallback0 = () => void;
type WorkerCallback1 = (arg1: any) => void;
type WorkerCallback2 = (arg1: any, arg2: any) => void;
type WorkerCallback3 = (arg1: any, arg2: any, arg3: any) => void;
type WorkerCallback4 = (arg1: any, arg2: any, arg3: any, arg4: any) => void;

interface FarmOptions {
  maxCallsPerWorker?: number
  maxConcurrentWorkers?: number
  maxConcurrentCallsPerWorker?: number
  maxConcurrentCalls?: number
  maxCallTime?: number
  maxRetries?: number
  autoStart?: boolean
}

interface WorkerFarm {
  (name: string): Workers;
  (name: string, exportedMethods: string[]): Workers;
  (options: FarmOptions, name: string): Workers;
  (options: FarmOptions, name: string, exportedMethods: string[]): Workers;

  end: (workers: Workers) => void;
}

declare module "worker-farm" {
  const workerFarm: WorkerFarm;
  export = workerFarm;
}
