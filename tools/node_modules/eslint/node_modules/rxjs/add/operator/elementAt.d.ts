import { elementAt } from '../../operator/elementAt';
declare module '../../Observable' {
    interface Observable<T> {
        elementAt: typeof elementAt;
    }
}
