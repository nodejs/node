# How to use Node.js as a shared library
## Limitations
* It is only possible to access Node.js from the same thread (no multi-threading).
* There can only be one Node.js per process at the same time.

## Handling the Node.js event loop
There are two different ways of handling the Node.js event loop.
### C++ keeps control over thread
By calling `node::ProcessEvents()`, the Node.js event loop will be run once, handling the next pending event. The return value of the call specifies whether there are more events in the queue.

### C++ gives control of the thread to Node.js
By calling `node::RunEventLoop(callback)`, the C++ host program gives up the control of the thread and allows the Node.js event loop to run until no more events are in the queue or `node::StopEventLoop()` is called. The `callback` parameter in the `RunEventLoop` function is called once per iteration of the event loop. This allows the C++ programmer to react to changes in the Node.js state and e.g. terminate Node.js preemptively.

## Examples
In the following, a few examples demonstrate the usage of Node.js as a library. For more complex examples, including handling of the event loop, see the [node-embed](https://github.com/hpicgs/node-embed) repository.

### (1) Evaluating in-line JavaScript code
This example evaluates multiple lines of JavaScript code in the global Node.js context. The result of `console.log` is piped to stdout.

```C++
node::Initialize();
node::Evaluate("var helloMessage = 'Hello from Node.js!';");
node::Evaluate("console.log(helloMessage);");
```

### (2) Running a JavaScript file
This example evaluates a JavaScript file and lets Node handle all pending events until the event loop is empty.

```C++
node::Initialize();
node::Run("cli.js");
while (node::ProcessEvents()) { }
``` 

### (3) Including an NPM Module
This example uses the [fs](https://nodejs.org/api/fs.html) module to check whether a specific file exists.
```C++
node::Initialize();
auto fs = node::IncludeModule("fs");
v8::Isolate *isolate = node::internal::isolate();

// Check if file cli.js exists in the current working directory.
auto result = node::Call(fs, "existsSync", {v8::String::NewFromUtf8(isolate, "file.txt")});

auto file_exists = v8::Local<v8::Boolean>::Cast(result)->BooleanValue();
std::cout << (file_exists ? "file.txt exists in cwd" : "file.txt does NOT exist in cwd") << std::endl;

```

### (4) Advanced: Combining a Qt GUI with Node.js
This example, which is borrowed from the examples repository [node-embed](https://github.com/hpicgs/node-embed), fetches a RSS feed from the BBC and displays it in a Qt GUI. For this, the `feedparser` and `request` modules from NPM are utilized.

#### main.cpp
```C++
#include <chrono>
#include <iostream>
#include <thread>

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cpplocate/cpplocate.h>

#include "node.h"
#include "node_lib.h"

#include "RssFeed.h"

int main(int argc, char* argv[]) {
  // Locate the JavaScript file we want to embed:
  const std::string js_file = "data/node-lib-qt-rss.js";
  const std::string data_path = cpplocate::locatePath(js_file);
  const std::string js_path = data_path + "/" + js_file;

  // Initialize Qt:
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QGuiApplication app(argc, argv);
  QQmlApplicationEngine engine;

  // to be able to access the public slots of the RssFeed instance
  // we inject a pointer to it in the QML context:
  engine.rootContext()->setContextProperty("rssFeed", &RssFeed::getInstance());

  engine.load(QUrl(QLatin1String("qrc:/main.qml")));

  // Initialize Node.js engine:
  node::Initialize();

  // Register C++ methods to be used within JavaScript
  // The third parameter binds the C++ module to that name,
  // allowing the functions to be called like this: "cppQtGui.clearFeed(...)"
  node::RegisterModule("cpp-qt-gui", {
                         {"addFeedItem", RssFeed::addFeedItem},
                         {"clearFeed", RssFeed::clearFeed},
                         {"redraw", RssFeed::redrawGUI},
                       }, "cppQtGui");

  // Evaluate the JavaScript file once:
  node::Run(js_path);

  // Load intial RSS feed to display it:
  RssFeed::refreshFeed();

  // Run the Qt application:
  app.exec();

  // After we are done, deinitialize the Node.js engine:
  node::Deinitialize();
}
```

#### RssFeed.h
```C++
#pragma once

#include <QObject>
#include "node.h"

/**
 * @brief The RssFeed class retrieves an RSS feed from the Internet and
 * provides its entries.
 */
class RssFeed : public QObject {

  Q_OBJECT

  Q_PROPERTY(QStringList entries READ getEntries NOTIFY entriesChanged)

private:
  /**
   * @brief Creates a new RssFeed with a given QObject as its parent.
   *
   * @param parent The parent object.
   */
  explicit RssFeed(QObject* parent=nullptr);

public:
  /**
   * @brief Returns the singleton instance for this class.
   *
   * @return The singlton instance for this class.
   */
  static RssFeed& getInstance();

  /**
   * @brief This method is called from the embedded JavaScript.
   * It is used for deleting all items from this RSS feed.
   *
   * @param args The arguments passed from the embedded JavaScript.
   * Hint: This method does not expect any arguments.
   */
  static void clearFeed(const v8::FunctionCallbackInfo<v8::Value>& args);

  /**
   * @brief This method is called from the embedded JavaScript.
   * It is used to add an entry to this RSS feed.
   *
   * @param args The arguments passed from the embedded JavaScript.
   * Hint: This method expects an object, which contains the RSS feed item.
   */
  static void addFeedItem(const v8::FunctionCallbackInfo<v8::Value>& args);

  /**
   * @brief This method is called from the embedded JavaScript.
   * It is used to refresh the GUI, after all retrieved feed items have been
   * appended.
   *
   * @param args The arguments passed from the embedded JavaScript.
   * Hint: This method does not expect any arguments.
   */
  static void redrawGUI(const v8::FunctionCallbackInfo<v8::Value>& args);

  /**
   * @brief This method updates the feed by calling a JS function
   * and running the Node.js main loop until the whole feed was received.
   */
  Q_INVOKABLE static void refreshFeed();

private:
  static RssFeed* instance;   /*!< The singleton instance for this class. */
  QStringList entries;        /*!< The list of RSS feeds to display. */

signals:
  void entriesChanged();      /*!< Emitted after entries were added or removed. */

public slots:
  /**
   * @brief getEntries returns the entries of the RSS feed
   *
   * @return a list of text entries
   */
  QStringList getEntries() const;
};
```

#### RssFeed.cpp
```C++
#include "RssFeed.h"

#include <iostream>
#include <QGuiApplication>
#include "node_lib.h"

RssFeed* RssFeed::instance = nullptr;

RssFeed::RssFeed(QObject* parent)
  : QObject(parent)
{

}

RssFeed& RssFeed::getInstance(){
  if (instance == nullptr){
    instance = new RssFeed();
  }
  return *instance;
}

QStringList RssFeed::getEntries() const {
  return entries;
}

void RssFeed::clearFeed(const v8::FunctionCallbackInfo<v8::Value>& args) {
  getInstance().entries.clear();
}

void RssFeed::redrawGUI(const v8::FunctionCallbackInfo<v8::Value>& args) {
  emit getInstance().entriesChanged();
}

void RssFeed::refreshFeed() {
  // invoke the embedded JavaScript in order to fetch new RSS feeds:
  node::Evaluate("emitRequest()");

  // wait for the embedded JavaScript to finish its execution
  // meanwhile, process any QT events
  node::RunEventLoop([](){ QGuiApplication::processEvents(); });
}

void RssFeed::addFeedItem(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // check, whether this method was called as expected
  // therefore, we need to make sure, that the first argument exists
  // and that it is an object
  v8::Isolate* isolate = args.GetIsolate();
  if (args.Length() < 1 || !args[0]->IsObject()) {
    isolate->ThrowException(v8::Exception::TypeError(
                              v8::String::NewFromUtf8(isolate, "Error: One object expected")));
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj = args[0]->ToObject(context).ToLocalChecked();

  // we want to get all properties of our input object:
  v8::Local<v8::Array> props = obj->GetOwnPropertyNames(context).ToLocalChecked();

  for (int i = 0, l = props->Length(); i < l; i++) {
    v8::Local<v8::Value> localKey = props->Get(i);
    v8::Local<v8::Value> localVal = obj->Get(context, localKey).ToLocalChecked();
    std::string key = *v8::String::Utf8Value(localKey);
    std::string val = *v8::String::Utf8Value(localVal);

    // append the RSS feed body to the list of RSS feeds:
    getInstance().entries << QString::fromStdString(val);
  }
}
```

#### node-lib-qt-rss.js
```JS
var FeedParser = require('feedparser');
var request = require('request'); // for fetching the feed

var emitRequest = function () {
  console.log("Refreshing feeds...")
  var feedparser = new FeedParser([]);
  var req = request('http://feeds.bbci.co.uk/news/world/rss.xml')

  req.on('error', function (error) {
    // catch all request errors but don't handle them in this demo
  });

  req.on('response', function (res) {
    var stream = this; // `this` is `req`, which is a stream

    if (res.statusCode !== 200) {
      this.emit('error', new Error('Bad status code'));
    }
    else {
      cppQtGui.clearFeed();
      stream.pipe(feedparser);
    }
  });

  feedparser.on('error', function (error) {
    // catch all parser errors but don't handle them in this demo
  });

  feedparser.on('readable', function () {
    var stream = this; // `this` is `feedparser`, which is a stream
    var item = stream.read();

    if (item) {
      var itemString = item['title'] + '\n' + item['description'];
      cppQtGui.addFeedItem( { item: itemString } );
    }
  });

  feedparser.on('end', function (){
    cppQtGui.redraw();
  });
}
```
