let counter = 0;

export async function initialize() {
  counter += 1;
  console.log('hooks initialize', counter);
  return counter;
}
