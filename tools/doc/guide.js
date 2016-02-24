const path = require('path');
const fs = require('fs');

const remark = require('remark');
const html = require('remark-html');
const toc = require('remark-toc');
const hljs = require('remark-highlight.js');

const processor = remark()
  .use(toc)
  .use(html)
  .use(hljs)
const remarkOptions = {
  yaml: true,
  bullet: '*'
}

const guidesDir = path.join(__dirname, '../../', 'doc', 'guides');
const outputDir = path.join(__dirname, '../../', 'out', 'doc', 'guides');
const templateFile = path.join(__dirname, '../../', 'doc', 'template-guide.html');
const template = fs.readFileSync(templateFile, { encoding: 'utf8' })

fs.readdir(guidesDir, function(err, files) {
  if (err) {
    throw err;
  }
  files.forEach(function(fileName) {
    const pageSlug = path.basename(fileName, '.md');
    fs.readFile(path.join(guidesDir, fileName), { encoding: 'utf8' }, function(err, content) {
      var mdast = processor.parse(content, remarkOptions);
      mdast = processor.run(mdast);
      const html = processor.stringify(mdast, remarkOptions);
      // Locate YAML front matter
      if (mdast.children[0] && mdast.children[0].type === 'yaml') {
        // Found YAML
        var yamlData = {}
        mdast.children[0].value.split('\n').forEach(function(line) {
          const keyValue = line.match(/(\w+):\s?(.+)/)
          yamlData[keyValue[1]] = keyValue[2]
        })
        // console.log(yamlData)
      }
      var output = template.replace(/__VERSION__/g, process.version);
      output = output.replace(/__CONTENT__/g, html);
      output = output.replace(/__FILENAME__/g, pageSlug);
      fs.writeFile(path.join(outputDir, pageSlug + '.html'), output, function(err) {
        if (err) {
          throw err;
        }
      })
    })
  })
})