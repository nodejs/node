import { defer as staticDefer } from '../../observable/defer';
declare module '../../Observable' {
    namespace Observable {
        let defer: typeof staticDefer;
    }
}
