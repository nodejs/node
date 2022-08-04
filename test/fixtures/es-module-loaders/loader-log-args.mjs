export async function resolve(...args) {
  console.log(`resolve arg count: ${args.length}`);
  console.log({
    specifier: args[0],
    context: args[1],
    next: args[2],
  });

  return {
    shortCircuit: true,
    url: args[0],
  };
}

export async function load(...args) {
  console.log(`load arg count: ${args.length}`);
  console.log({
    url: args[0],
    context: args[1],
    next: args[2],
  });

  return {
    format: 'module',
    source: '',
    shortCircuit: true,
  };
}
