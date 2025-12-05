import http from 'node:http';

let restore;
if (process.env.SET_GLOBAL_PROXY) {
  const config = JSON.parse(process.env.SET_GLOBAL_PROXY);
  restore = http.setGlobalProxyFromEnv(config);
}

const response = await fetch(process.env.FETCH_URL);
const body = await response.text();
console.log(body);

if (process.env.CLEAR_GLOBAL_PROXY_AND_RETRY) {
  restore();
  const response = await fetch(process.env.FETCH_URL);
  const body = await response.text();
  console.log(body);
}
