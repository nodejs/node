const fs = require('fs');
const path = require('path');

// const base = path.join(__dirname, 'parallel');
// const files = fs.readdirSync(base);

// const MapOne = [];
// for (const file of files) {
//     const filename = path.basename(file).split('.')[0];
//     MapOne.push([filename, path.join(base, file)]);
// }

// // Copy to the 'new_parallel' directory
// const new_base = path.join(__dirname, 'new_parallel');

// fs.rmSync(new_base, { recursive: true, force: true });
// fs.mkdirSync(new_base);

// const DIR_DEPTH = 1;
// const UPDATE_EXT = ['.js', '.mjs', '.cjs'];


// for (const [filename, file] of MapOne) {
//     const parts = filename.split('-');
//     if (parts[0] === 'test') parts.shift();
//     let testname = parts.pop();
//     let cur = new_base;
//     let depth = 0;

//     for (const part of parts) {
//         if (depth >= DIR_DEPTH) {
//             testname = `${part}-${testname}`
//         } else {
//             cur = path.join(cur, part);
//             if (!fs.existsSync(cur)) {
//                 fs.mkdirSync(cur);
//             }
//         }
//         depth++;
//     }

//     // if depth is LESS than DIR_DEPTH, than our test becomes 'testname/index.[ext]'
//     const ext = path.extname(file);
//     if (depth < DIR_DEPTH && UPDATE_EXT.includes(ext)) {
//         cur = path.join(cur, testname);
//         if (!fs.existsSync(cur)) {
//             fs.mkdirSync(cur);
//         }
//         testname = 'index';
//     }

//     copyFileWithImports(file, path.join(cur, `${testname}${ext}`));
// }

// function updateRequirePath(line, requirePath, depth) {
//     const originalRequirePath = requirePath;
//     // Add a '../' for each directory level we need to go up
//     for (let i = 0; i < depth; i++) {
//         requirePath = `../${requirePath}`;
//     }
//     return line.replace(originalRequirePath, requirePath);
// }

// function copyFileWithImports(src, dest) {
//     const ext = path.extname(src);
//     if (!UPDATE_EXT.includes(ext)) {
//         fs.copyFileSync(src, dest);
//         return;
//     }

//     const content = fs.readFileSync(src).toString();
//     const lines = content.split('\n');

//     const depth = dest.split('/new_parallel/')[1].split(path.sep).length - 1;

//     for (let i = 0; i < lines.length; i++) {
//         let line = lines[i];
//         const require = line.match(/require\(['"](.+)['"]\)/);
//         // if the require starts with a '.', it's a relative path,
//         // so we need to modify it to point to the correct file
//         if (require && require[1].startsWith('.')) {
//             lines[i] = updateRequirePath(line, require[1], depth);
//         }
//     }

//     fs.writeFileSync(dest, lines.join('\n'));
// }

function recursiveListFiles(dir, fileList = []) {
    const files = fs.readdirSync(dir);
    for (const file of files) {
        const filePath = path.join(dir, file);
        if (fs.statSync(filePath).isDirectory()) {
            recursiveListFiles(filePath, fileList);
        } else {
            fileList.push(filePath);
        }
    }
    return fileList;
}

const readline = require('readline');

// Find all the files with more than one '-' in the name (because we named them incorrectly)
// use readline to ask the user for the correct name
const files = recursiveListFiles(path.join(
    __dirname,
    "parallel"
));

const MapTwo = [];
for (const file of files) {
    const filename = path.basename(file).split('.')[0];
    if (filename.split('-').length > 2) {
        let parts = filename.split('-');
        let correctName = parts.pop();
        while (parts.length > 0) {
            correctName = `${parts.shift()}-${correctName}`;
        }

        console.log(`Incorrect name: ${filename}`);
        console.log(`Correct name: ${correctName}`);
        const ext = path.extname(file);

        fs.renameSync(file, path.join(path.dirname(file), `${correctName}${ext}`));
    }
}

console.log(MapTwo[0])