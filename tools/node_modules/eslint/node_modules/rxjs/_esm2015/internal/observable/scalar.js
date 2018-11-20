import { Observable } from '../Observable';
export function scalar(value) {
    const result = new Observable(subscriber => {
        subscriber.next(value);
        subscriber.complete();
    });
    result._isScalar = true;
    result.value = value;
    return result;
}
//# sourceMappingURL=scalar.js.map