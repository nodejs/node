import { fromPromise as staticFromPromise } from '../../observable/fromPromise';
declare module '../../Observable' {
    namespace Observable {
        let fromPromise: typeof staticFromPromise;
    }
}
