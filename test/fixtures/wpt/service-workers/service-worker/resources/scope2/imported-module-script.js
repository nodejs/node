export const imported = 'A module script.';
onmessage = msg => {
    msg.source.postMessage('pong');
};
