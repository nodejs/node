import crypto from 'crypto';
import path from 'path';

const shaRegExp = /-sha512(\.mjs)$/;

function getSourcePath(p) {
  if (p.protocol !== `file:`)
    return p;

  const pString = p.toString();
  const pFixed = pString.replace(shaRegExp, `$1`);
  if (pFixed === pString)
    return p;

  return new URL(pFixed);
}

export function getFileSystem(defaultGetFileSystem) {
  const fileSystem = defaultGetFileSystem();

  return {
    readFileSync(p) {
      const fixedP = getSourcePath(p);
      if (fixedP === p)
        return fileSystem.readFileSync(p);

      const content = fileSystem.readFileSync(fixedP);
      const hash = crypto.createHash(`sha512`).update(content).digest(`hex`);

      return Buffer.from(`export default ${JSON.stringify(hash)};`);
    },

    statEntrySync(p) {
      const fixedP = getSourcePath(p);
      return fileSystem.statEntrySync(fixedP);
    },

    realpathSync(p) {
      const fixedP = getSourcePath(p);
      if (fixedP === p)
        return fileSystem.realpathSync(p);

      const realpath = fileSystem.realpathSync(fixedP);
      if (path.extname(realpath) !== `.mjs`)
        throw new Error(`Paths must be .mjs extension to go through the sha512 loader`);

      return realpath.replace(/\.mjs$/, `-sha512.mjs`);
    },
  };
}
