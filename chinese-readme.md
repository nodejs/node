# Node.js

Node.js 是一个开源、跨平台的 JavaScript 运行时环境。

有关 Node.js 的使用信息，请参阅 [Node.js 官方网站](https://nodejs.org/)。

Node.js 项目采用[开放治理模式](https://github.com/nodejs/node/blob/main/GOVERNANCE.md)。[OpenJS 基金会](https://openjsf.org/)为该项目提供支持。

贡献者应以协作的方式参与项目，共同推动项目发展。我们鼓励建设性地交流不同意见，并寻求妥协。[技术指导委员会（TSC）](https://github.com/nodejs/node/blob/main/GOVERNANCE.md#technical-steering-committee)保留限制或禁止那些反复以阻碍、消耗或其他负面方式影响其他参与者的贡献者参与项目的权利。

**本项目有一份[行为准则（Code of Conduct）](https://github.com/nodejs/admin/blob/HEAD/CODE_OF_CONDUCT.md)。**

## 目录

* [支持](#支持)
* [发布类型](#发布类型)

  * [下载](#下载)

    * [Current 和 LTS 版本](#current-和-lts-版本)
    * [Nightly 每夜构建](#nightly-每夜构建)
    * [API 文档](#api-文档)
  * [验证二进制文件](#验证二进制文件)
* [构建 Node.js](#构建-nodejs)
* [安全](#安全)
* [为 Node.js 做贡献](#为-nodejs-做贡献)
* [当前项目团队成员](#当前项目团队成员)

  * [TSC（技术指导委员会）](#tsc技术指导委员会)
  * [协作者](#协作者)
  * [问题分流人员](#问题分流人员)
  * [发布密钥](#发布密钥)
* [许可证](#许可证)

## 支持

如果你需要帮助，请查看[获取支持的说明](https://github.com/nodejs/node/blob/main/.github/SUPPORT.md)。

## 发布类型

* **Current（当前版本）**：处于积极开发阶段。Current 版本的代码位于对应主要版本号的分支中，例如 [v22.x](https://github.com/nodejs/node/tree/v22.x)。Node.js 每 6 个月发布一个新的主要版本，通常在每年 4 月和 10 月进行。每年 10 月发布的主要版本支持周期为 8 个月；每年 4 月发布的主要版本会在当年 10 月转为 LTS。

* **LTS（长期支持版）**：提供长期支持，重点关注稳定性和安全性。每个偶数编号的主要版本最终都会成为 LTS 版本。LTS 版本会获得 12 个月的 Active LTS 支持，随后获得 18 个月的 Maintenance（维护）支持。LTS 版本线使用按字母顺序排列的代号，从 v4 Argon 开始。除特殊情况外，LTS 版本不会加入破坏性变更或新功能。

* **Nightly（每夜构建版）**：根据 Current 分支的代码每天构建一次，只要当天存在代码变化就会生成新的构建版本。使用时请谨慎。

Current 和 LTS 版本遵循[语义化版本规范（Semantic Versioning）](https://semver.org/)。每个 Current 和 LTS 版本都会由 Node.js 发布团队成员进行签名。更多信息请参阅 [Release README](https://github.com/nodejs/Release#readme)。

### 下载

Node.js 的二进制文件、安装程序和源代码压缩包可以从以下地址获取：

[https://nodejs.org/en/download/](https://nodejs.org/en/download/)

#### Current 和 LTS 版本

[https://nodejs.org/download/release/](https://nodejs.org/download/release/)

`latest` 目录是最新 Current 版本的别名。

`latest-*代号*` 目录是对应 LTS 版本线最新版本的别名。例如，`latest-hydrogen` 目录包含最新的 Hydrogen（Node.js 18）版本。

#### Nightly 每夜构建

[https://nodejs.org/download/nightly/](https://nodejs.org/download/nightly/)

每个目录和文件名都包含版本号、UTC 日期以及 HEAD 提交的简短 SHA。

例如：

```text
v22.0.0-nightly20240424ddd0a9e494
```

其中：

* `v22.0.0` 是版本号
* `20240424` 是 UTC 日期
* `ddd0a9e494` 是 HEAD 提交的简短 Commit SHA

#### API 文档

最新 Current 版本的 API 文档位于：

[https://nodejs.org/api/](https://nodejs.org/api/)

特定版本的 API 文档可以在对应发布目录的 `docs` 子目录中找到。

特定版本的文档也可以在以下地址找到：

[https://nodejs.org/download/docs/](https://nodejs.org/download/docs/)

### 验证二进制文件

下载目录包含一个 `SHASUMS256.txt.asc` 文件，其中包含 SHA 校验和以及发布者的 PGP 签名。

你可以从 nodejs/release-keys 获取可信密钥环，例如使用 `curl`：

```bash
curl -fsLo "/path/to/nodejs-keyring.kbx" "https://github.com/nodejs/release-keys/raw/HEAD/gpg/pubring.kbx"
```

或者，你可以将发布者的密钥导入默认密钥环。

如果使用默认密钥环，请传入：

```text
--keyring="${GNUPGHOME:-~/.gnupg}/pubring.kbx"
```

然后，可以按照以下方式验证下载的 Node.js 文件：

```bash
curl -fsO "https://nodejs.org/dist/${VERSION}/SHASUMS256.txt.asc" \
&& gpgv --keyring="/path/to/nodejs-keyring.kbx" --output SHASUMS256.txt < SHASUMS256.txt.asc \
&& shasum --check SHASUMS256.txt --ignore-missing
```

更多信息请参阅[验证二进制文件](https://github.com/nodejs/node#verifying-binaries)。

## 构建 Node.js

请参阅 [BUILDING.md](https://github.com/nodejs/node/blob/main/BUILDING.md)，其中包含从源代码构建 Node.js 的说明以及支持的平台列表。

## 安全

有关如何报告 Node.js 安全漏洞的信息，请参阅 [SECURITY.md](https://github.com/nodejs/node/blob/main/SECURITY.md)。

## 为 Node.js 做贡献

* [为项目做贡献](https://github.com/nodejs/node/blob/main/CONTRIBUTING.md)
* [工作组](https://github.com/nodejs/TSC/blob/HEAD/WORKING_GROUPS.md)
* [战略计划](https://github.com/nodejs/node/blob/main/doc/contributing/strategic-initiatives.md)
* [技术价值与优先级](https://github.com/nodejs/node/blob/main/doc/contributing/technical-values.md)

## 当前项目团队成员

有关 Node.js 项目治理的信息，请参阅 [GOVERNANCE.md](https://github.com/nodejs/node/blob/main/GOVERNANCE.md)。

### TSC（技术指导委员会）

有关 TSC 投票成员、正式成员和荣誉成员的完整列表，请参阅：

[https://github.com/nodejs/node/blob/main/GOVERNANCE.md](https://github.com/nodejs/node/blob/main/GOVERNANCE.md)

### 协作者

协作者负责维护 Node.js 项目。

他们应遵循[协作者指南](https://github.com/nodejs/node/blob/main/doc/contributing/collaborator-guide.md)。

### 问题分流人员

问题分流人员负责协助处理和分类新提交的问题。

他们应遵循[问题分流指南](https://github.com/nodejs/node/blob/main/doc/contributing/issues.md#triaging-a-bug-report)。

### 发布密钥

Node.js 发布人员使用主要 GPG 密钥对部分发布版本进行签名。

项目维护了一个用于验证当前 Node.js 发布版本的密钥环：

[https://github.com/nodejs/release-keys/raw/refs/heads/main/gpg-only-active-keys/pubring.kbx](https://github.com/nodejs/release-keys/raw/refs/heads/main/gpg-only-active-keys/pubring.kbx)

也可以从公共密钥服务器导入发布密钥。

例如：

```bash
gpg --keyserver hkps://keys.openpgp.org --recv-keys 5BE8A3F6C8A5C01D106C0AD820B1A390B168D356
gpg --keyserver hkps://keys.openpgp.org --recv-keys DD792F5973C6DE52C432CBDAC77ABFA00DDBF2B7
gpg --keyserver hkps://keys.openpgp.org --recv-keys CC68F5A3106FF448322E48ED27F5E38D5B0A215F
gpg --keyserver hkps://keys.openpgp.org --recv-keys 8FCCA13FEF1D0C2E91008E09770F7A9A5AE15600
gpg --keyserver hkps://keys.openpgp.org --recv-keys 890C08DB8579162FEE0DF9DB8BEAB4DFCF555EF4
gpg --keyserver hkps://keys.openpgp.org --recv-keys C82FA3AE1CBEDC6BE46B9360C43CEC45C17AB93C
gpg --keyserver hkps://keys.openpgp.org --recv-keys 108F52B48DB57BB0CC439B2997B01419BD92F80A
gpg --keyserver hkps://keys.openpgp.org --recv-keys 655F3B5C1FB3FA8D1A0CA6BDE4A7D232B936D2FD
gpg --keyserver hkps://keys.openpgp.org --recv-keys A363A499291CBBC940DD62E41F10027AF002F8B0
```

有关如何使用这些密钥验证 Node.js 下载文件的信息，请参阅[验证二进制文件](https://github.com/nodejs/node#verifying-binaries)。

项目还维护了一个可以验证所有历史 Node.js 发布版本的密钥环：

[https://github.com/nodejs/release-keys/raw/refs/heads/main/gpg/pubring.kbx](https://github.com/nodejs/release-keys/raw/refs/heads/main/gpg/pubring.kbx)

## 许可证

Node.js 使用 [MIT 许可证](https://opensource.org/licenses/MIT)。

该项目还依赖一些可能使用不同开源许可证的外部库。有关所有包含的许可证，请参阅 [LICENSE](https://github.com/nodejs/node/blob/main/LICENSE) 文件。

如果你正在贡献文档或源代码，请确保你的新增内容符合项目的许可证要求。
