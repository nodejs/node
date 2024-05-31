import { threadId } from "worker_threads";
import esm from "node:internal/modules/esm/loader";

export function initialize(data, context) {
  process.on('beforeExit', () => {
    process._rawDebug(JSON.stringify({
      operation: "chains",
      names: Array.from(esm.getOrInitializeCascadedLoader().getCustomizations().getChains().keys()),
    }));
  })

  console.log(JSON.stringify({
    operation: "initialize",
    threadId: context.threadId,
    data,
    workerData: context.workerData,
  }));
}

export function resolve(specifier, context, next) {
  if(specifier.startsWith('node:') || specifier.startsWith('../')) {
    return next(specifier);    
  }
  
  console.log(JSON.stringify({
    operation: "resolve",
    specifier,
    threadId: context.threadId,
    data: context.data,
    workerData: context.workerData,
  }));

  return next(specifier);
}