const chalk = require('chalk');
const columns = require('.');

const values = [
	'blue' + chalk.bgBlue('berry'),
	'笔菠萝' + chalk.yellow('苹果笔'),
	chalk.red('apple'), 'pomegranate',
	'durian', chalk.green('star fruit'),
	'パイナップル', 'apricot', 'banana',
	'pineapple', chalk.bgRed.yellow('orange')
];

console.log('');
console.log(columns(values));
console.log('');
