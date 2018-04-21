import { SocksClientOptions, SocksClientChainOptions } from '../client/socksclient';
/**
 * Validates the provided SocksClientOptions
 * @param options { SocksClientOptions }
 * @param acceptedCommands { string[] } A list of accepted SocksProxy commands.
 */
declare function validateSocksClientOptions(options: SocksClientOptions, acceptedCommands?: string[]): void;
/**
 * Validates the SocksClientChainOptions
 * @param options { SocksClientChainOptions }
 */
declare function validateSocksClientChainOptions(options: SocksClientChainOptions): void;
export { validateSocksClientOptions, validateSocksClientChainOptions };
