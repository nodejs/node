/** PURE_IMPORTS_START .._.._Observable,.._.._operator_mergeMap PURE_IMPORTS_END */
import { Observable } from '../../Observable';
import { mergeMap } from '../../operator/mergeMap';
Observable.prototype.mergeMap = mergeMap;
Observable.prototype.flatMap = mergeMap;
//# sourceMappingURL=mergeMap.js.map
