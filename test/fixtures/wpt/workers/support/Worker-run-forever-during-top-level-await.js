await 1;
postMessage('start');
onerror = () => postMessage('onerror');
while(1);
