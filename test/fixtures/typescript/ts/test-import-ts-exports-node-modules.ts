import { greeting } from 'typescript-pkg';
import { sub } from 'typescript-pkg/subpath';

const message: string = greeting.message;

console.log(message);
console.log(sub);
