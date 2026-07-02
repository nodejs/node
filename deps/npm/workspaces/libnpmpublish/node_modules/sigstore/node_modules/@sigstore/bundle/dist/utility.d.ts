type ValueOf<Obj> = Obj[keyof Obj];
type OneOnly<Obj, K extends keyof Obj> = {
    [key in Exclude<keyof Obj, K>]: undefined;
} & {
    [key in K]: Obj[K];
};
type OneOfByKey<Obj> = {
    [key in keyof Obj]: OneOnly<Obj, key>;
};
export type OneOf<T> = ValueOf<OneOfByKey<T>>;
export type WithRequired<T, K extends keyof T> = T & {
    [P in K]-?: NonNullable<T[P]>;
};
export {};
