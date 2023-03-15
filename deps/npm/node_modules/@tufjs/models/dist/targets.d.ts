import { MetadataKind, Signed, SignedOptions } from './base';
import { Delegations } from './delegations';
import { TargetFile } from './file';
import { JSONObject } from './utils';
type TargetFileMap = Record<string, TargetFile>;
interface TargetsOptions extends SignedOptions {
    targets?: TargetFileMap;
    delegations?: Delegations;
}
export declare class Targets extends Signed {
    readonly type = MetadataKind.Targets;
    readonly targets: TargetFileMap;
    readonly delegations?: Delegations;
    constructor(options: TargetsOptions);
    addTarget(target: TargetFile): void;
    equals(other: Targets): boolean;
    toJSON(): JSONObject;
    static fromJSON(data: JSONObject): Targets;
}
export {};
