export interface ISetCtor {
    new <T>(): ISet<T>;
}
export interface ISet<T> {
    add(value: T): void;
    has(value: T): boolean;
    size: number;
    clear(): void;
}
export declare function minimalSetImpl<T>(): ISetCtor;
export declare const Set: ISetCtor;
