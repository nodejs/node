# Usage and example

## Usage

<!--introduced_in=v0.10.0-->

<!--type=misc-->

`node [options] [V8 options] [script.js | -e "script" | - ] [arguments]`

Please see the [Command-line options][] document for more information.

## Example

An example of a [web server][] written with Node.js which responds with
`'Hello, World!'`:

Commands in this document start with `$` or `>` to replicate how they would
appear in a user's terminal. Do not include the `$` and `>` characters. They are
there to show the start of each command.

Lines that don't start with `$` or `>` character show the output of the previous
command.

First, make sure to have downloaded and installed Node.js. See
[Installing Node.js via package manager][] for further install information.

Now, create an empty project folder called `projects`, then navigate into it.

Linux and Mac:

```bash
mkdir ~/projects
cd ~/projects
```

Windows CMD:

```powershell
mkdir %USERPROFILE%\projects
cd %USERPROFILE%\projects
```

Windows PowerShell:

```powershell
mkdir $env:USERPROFILE\projects
cd $env:USERPROFILE\projects
```

Next, create a new source file in the `projects`
folder and call it `hello-world.js`.

Open `hello-world.js` in any preferred text editor and
paste in the following content:

```js
// Import the 'node:http' (or just 'http') module, which provides functionality to create/request HTTP servers.
const http = require('node:http');

// Define the hostname and port number on which the server will listen for requests.
const hostname = '127.0.0.1'; // localhost
const port = 3000;

// Create a new HTTP server instance using the 'createServer' method.
const server = http.createServer((req, res) => {
  // Set the HTTP status code to 200, indicating success.
  res.statusCode = 200;

  // Set the response header to specify the content type as plain text.
  res.setHeader('Content-Type', 'text/plain');

  // Send back 'Hello, World' to the client
  res.end('Hello, World!\n');
});

// Tell the server to listen for incoming connections on the specified hostname and port.
server.listen(port, hostname, () => {
  // Once the server starts listening, log a message to the console indicating the server is running.
  console.log(`Server running at http://${hostname}:${port}/`);
});

```

Save the file. Then, in the terminal window, to run the `hello-world.js` file,
enter:

```bash
node hello-world.js
```

Output like this should appear in the terminal:

```console
Server running at http://127.0.0.1:3000/
```

Now, open any preferred web browser and visit `http://127.0.0.1:3000`.

If the browser displays the string `Hello, World!`, that indicates
the server is working.

[Command-line options]: cli.md#options
[Installing Node.js via package manager]: https://nodejs.org/en/download/package-manager/
[web server]: http.md
