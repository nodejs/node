import { concat as concatStatic } from '../../observable/concat';
declare module '../../Observable' {
    namespace Observable {
        let concat: typeof concatStatic;
    }
}
