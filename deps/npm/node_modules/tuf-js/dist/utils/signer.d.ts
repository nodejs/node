import crypto from 'crypto';
import { JSONObject } from '../utils/types';
export declare const verifySignature: (metaDataSignedData: JSONObject, key: crypto.VerifyKeyObjectInput, signature: string) => boolean;
