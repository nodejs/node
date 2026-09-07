var max_nesting_level = 8;

self.addEventListener('message', function(event) {
    var level = event.data;
    if (level < max_nesting_level)
      dispatchEvent(new MessageEvent('message', { data: level + 1 }));
    throw Error('error at level ' + level);
  });

self.addEventListener('install', function(event) {
    dispatchEvent(new MessageEvent('message', { data: 1 }));
  });
