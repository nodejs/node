# Usage

<!--introduced_in=v0.10.0-->
<!--type=misc-->

`node [options] [V8 options] [script.js | -e "script" | - ] [arguments]`

Please see the [Command Line Options][] document for information about
different options and ways to run scripts with Node.js.

## Example
An example of a [web server][] written with Node.js which responds with
`'Hello World!'`:

Firstly, Make sure you have downloaded Node.js from [Node.js Official website](http://nodejs.org/#download).
Then, Follow this [installation guide](https://nodejs.org/en/download/package-manager/).


Now, Create an empty project folder called `projects`, navigate into it:

Linux and Mac:

```txt
$ mkdir ~/projects
$ cd ~/projects
```
Windows CMD:
```txt
> mkdir %USERPROFILE%\projects
> cd %USERPROFILE%\projects
```

Windows PowerShell:

```txt
> mkdir $env:USERPROFILE\projects
> cd $env:USERPROFILE\projects
```

Next, create a new source file in the `projects` folder and call it `hello_world.js`.

If you’re using more than one word in your filename, use an underscore(`_`) to separate them for simplicity and avoid using the space character in file names. 

For example, you’d use `hello_world.js` rather than `hello world.js`.
Node.js files always end with the `.js` extension. 

open `hello_world.js` in your favorite text editor and paste in the following content.


```js
const http = require('http');

const hostname = '127.0.0.1';
const port = 3000;

const server = http.createServer((req, res) => {
  res.statusCode = 200;
  res.setHeader('Content-Type', 'text/plain');
  res.end('Hello World!\n');
});

server.listen(port, hostname, () => {
  console.log(`Server running at http://${hostname}:${port}/`);
});
```
Save the file, and go back to your terminal window enter the following command:

```txt
$ node hello_world.js
```
Your should see an output like this in your terminal to indicate Node.js server is running:
 ```javascript
 Server running at http://127.0.0.1:3000/
 ````
 Now, Open your favorite browser and visit `http://127.0.0.1:3000`.
 
 You should see the string `Hello, world!`. 

Many of the examples in the documentation can be run similarly.

[Command Line Options]: cli.html#cli_command_line_options
[web server]: http.html
