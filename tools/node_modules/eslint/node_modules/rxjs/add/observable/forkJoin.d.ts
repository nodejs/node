import { forkJoin as staticForkJoin } from '../../observable/forkJoin';
declare module '../../Observable' {
    namespace Observable {
        let forkJoin: typeof staticForkJoin;
    }
}
