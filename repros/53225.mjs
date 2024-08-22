import {serialize, deserialize} from 'node:v8';

const error = AbortSignal.abort().reason;
console.log(error instanceof DOMException); 
// true
console.log(error);
// DOMException [AbortError]: This operation was aborted
//     at new DOMException (node:internal/per_context/domexception:53:5)
//     at AbortSignal.abort (node:internal/abort_controller:205:14)
//     at ...

const clonedError = deserialize(serialize(error));
console.log(clonedError instanceof DOMException); 
// false
console.log(clonedError);
// {}