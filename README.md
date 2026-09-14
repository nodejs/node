# Mode

Mode 是基于 Node.js 的定制运行时。默认仍可像 Node.js 一样执行 JavaScript；在显式
启用后，它会在当前 Realm 注入轻量浏览器兼容环境，以运行依赖常见 DOM/BOM 的脚本，
而不需要嵌入 Chromium 或启动图形浏览器。

当前发布版：[`mode_20260914_v22.0.10`](https://github.com/wen2go/mode/releases/tag/mode_20260914_v22.0.10)。

## 与官方 Node.js 的区别

| 范围 | 官方 Node.js | Mode |
| --- | --- | --- |
| 启用方式 | 没有浏览器环境。 | 新增 `node:browser-env` 内置模块（CommonJS/ESM）和 `--browser-env-profile=file`。未启用时保持普通 Node.js 行为。 |
| DOM | 没有 `window`、`document` 或 HTML 树。 | 提供轻量 DOM、HTML 解析、节点/属性增删改、文本、查询 API、事件、`DOMParser` 及常用 HTML 元素构造器。 |
| BOM | 没有浏览器的 `location`、`history`、`navigator`、`screen`。 | 提供虚拟 `location/history`，可配置 UA、平台、语言、屏幕和窗口尺寸，以及 `NetworkInformation`、`BatteryManager`、MIME 类型、`sendBeacon()`。 |
| Cookie 与存储 | 没有浏览器 Cookie/Storage 接口。 | 提供内存 `document.cookie`、`localStorage`、`sessionStorage`。 |
| 浏览器特征 | 没有 `document.all`。 | 使用 Node/V8 内部 binding 提供特殊 `document.all` 语义，并为常见 `fn.toString()` 检查提供原生函数形态。 |
| 兼容存根 | 没有这些浏览器全局。 | 提供 `XMLHttpRequest` 状态/参数记录、`MutationObserver`、`indexedDB`、`chrome` 元数据、`open()`、`prompt()`、`msCrypto` 别名和确定性的 Canvas 表面。 |
| RS 本地服务 | 不提供。 | `rs_mode_server/server.js` 以每请求独立 Worker 执行挑战脚本；可使用原始 HTML、动态 UA、Cookie、Storage 和 Mode DOM/BOM。 |

## 安装与启动

从 [GitHub Releases](https://github.com/wen2go/mode/releases/tag/mode_20260914_v22.0.10)
下载与系统匹配的文件：

| 平台 | 文件 |
| --- | --- |
| macOS Apple Silicon | `mode_mac_arm_20260914_v22.0.10.tar.gz` |
| Linux x64 | `mode_linux_x64_20260914_v22.0.10.tar.gz` |
| Windows x64 | `mode_win_x64_20260914_v22.0.10.zip` |

macOS/Linux：

```bash
tar -xzf mode_linux_x64_20260914_v22.0.10.tar.gz
mkdir -p "$HOME/.local/bin"
install -m 755 mode_linux_x64_20260914_v22.0.10/mode "$HOME/.local/bin/mode"
export PATH="$HOME/.local/bin:$PATH"
mode -e "console.log(typeof require('node:browser-env').install)"
```

macOS Apple Silicon 请把文件和目录名替换为 `mode_mac_arm_20260914_v22.0.10`。
最后一条命令输出 `function` 即表示当前 shell 正在使用 Mode。

Windows：解压 ZIP 后运行：

```powershell
.\mode_win_x64_20260914_v22.0.10\mode.exe -e "console.log(typeof require('node:browser-env').install)"
```

使用 JSON profile 在目标脚本运行前安装浏览器环境：

```json
{
  "url": "https://example.test/",
  "html": "<div id=\"app\">hello</div>",
  "navigator": { "platform": "Win32", "languages": ["zh-CN", "zh"] }
}
```

```bash
mode --browser-env-profile=./browser-profile.json target.js
```

需要 getter、setter 或函数型自定义属性时，在包装脚本中先调用 `install()`，再加载
目标脚本：

```js
const { install } = require('node:browser-env');

install({
  url: 'https://example.test/',
  html: '<div id="app"></div>',
  navigator: { userAgent: 'Mozilla/5.0', platform: 'Win32' },
});

require('./target.js');
```

使用 RS 兼容服务：

```bash
mode rs_mode_server/server.js
```

服务默认监听 `http://127.0.0.1:8080`，提供 `GET /health` 与原有的
`POST /rs_env` 协议。

## 安全与能力边界

- Mode 是脚本兼容层，不是 Chromium：没有页面渲染、CSS/layout、真实 iframe 文档、
  Canvas 图像指纹、WebGL、WebRTC、AudioContext、浏览器 Worker API、真实导航，也不会
  自动执行初始 HTML 内的 `<script>`。
- `XMLHttpRequest` 只记录 `open()`/`send()` 的状态和参数，不会发出网络请求；Canvas
  为确定性兼容表面，不生成真实图形输出。
- `rs_mode_server` 会执行请求 HTML 关联的挑战 JavaScript；只应向受信任的本地调用方
  暴露，并只用于已获授权的目标。每个请求虽在独立 Worker 中运行，但这不是针对不可信
  JavaScript 的安全沙箱。
- `install({ hideNodeGlobals: true })` 仅隐藏可配置的 `process`、`require` 等常见 Node
  全局别名，用于减少环境差异；它不是权限隔离机制。普通 `mode` 进程仍具备与 Node.js
  相同的文件和网络权限。
- 当前版本已在授权目标的连续请求中得到 HTTP 200；这只说明该目标已读到的环境路径
  被覆盖，不代表所有网站都能通过浏览器指纹检测。

## 许可证

Mode 基于 Node.js 源码修改，沿用 [MIT License](./LICENSE)。仓库内第三方依赖可能有
各自的许可证；完整声明以 [LICENSE](./LICENSE) 为准。
