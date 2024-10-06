import { getPackageJSON } from 'node:module';

export default getPackageJSON(import.meta.resolve('..'));
