import fs from 'node:fs';
import { createInterface } from 'node:readline';

const dataFolder = new URL('../../doc/changelogs/', import.meta.url);

async function getVersionsFromFile(file) {
  const result = [];
  const input = fs.createReadStream(file);
  let toc = false;
  for await (const line of createInterface({
    input,
    crlfDelay: Infinity,
  })) {
    if (toc === false && line === '<table>') {
      toc = true;
    } else if (toc && line[0] !== '<') {
      input.close();
      return result;
    } else if (toc && line.startsWith('<a')) {
      result.push(line.slice(line.indexOf('>') + 1, -'</a><br/>'.length));
    } else if (toc && line.startsWith('<b><a')) {
      result.push(
        line.slice(line.indexOf('>', 3) + 1, -'</a></b><br/>'.length),
      );
    }
  }
  return result;
}

export async function getReleasedVersions() {
  const filesToCheck = [];

  const dir = await fs.promises.opendir(dataFolder);
  for await (const dirent of dir) {
    if (dirent.isFile()) {
      filesToCheck.push(getVersionsFromFile(new URL(dirent.name, dataFolder)));
    }
  }

  const versions = await Promise.all(filesToCheck);
  return versions.flat();
}
