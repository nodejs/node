import { combineLatest as combineLatestStatic } from '../../observable/combineLatest';
declare module '../../Observable' {
    namespace Observable {
        let combineLatest: typeof combineLatestStatic;
    }
}
