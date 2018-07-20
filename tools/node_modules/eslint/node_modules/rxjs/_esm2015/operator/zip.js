import { zip as higherOrder } from '../operators/zip';
/* tslint:enable:max-line-length */
/**
 * @param observables
 * @return {Observable<R>}
 * @method zip
 * @owner Observable
 */
export function zipProto(...observables) {
    return higherOrder(...observables)(this);
}
//# sourceMappingURL=zip.js.map