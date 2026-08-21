const response = await fetch(process.env.FETCH_URL);
const body = await response.text();
console.log(body);
