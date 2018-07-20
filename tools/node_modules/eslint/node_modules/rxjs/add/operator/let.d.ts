import { letProto } from '../../operator/let';
declare module '../../Observable' {
    interface Observable<T> {
        let: typeof letProto;
        letBind: typeof letProto;
    }
}
