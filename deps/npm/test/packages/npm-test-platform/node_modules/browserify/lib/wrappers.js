var fs = require('fs');

module.exports = fs.readdirSync(__dirname + '/../wrappers')
    .filter(function (file) { return file.match(/\.js$/) })
    .reduce(function (acc, file) {
        var name = file.replace(/\.js$/, '');
        acc[name] = fs.readFileSync(__dirname + '/../wrappers/' + file, 'utf8');
        return acc;
    }, {})
;
