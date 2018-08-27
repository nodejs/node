import { merge as mergeStatic } from '../../observable/merge';
declare module '../../Observable' {
    namespace Observable {
        let merge: typeof mergeStatic;
    }
}
