console.error('load fixtures/b/d.js');

let string = 'D';

exports.D = () => string;

process.on('exit', () => {
  string = 'D done';
});

