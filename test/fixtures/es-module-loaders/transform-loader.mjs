import { promisify } from 'util';

const delay = promisify(setTimeout);

export async function transformSource(url, source) {
  await delay(50);

  return source
    .replace(/'bar'/, "'transformed-bar'")
    .replace(/'foo'/, "'transformed-foo'");
}
