# 贡献指南

有许多方法可以为项目做出贡献：登记 bug、提交拉取请求、反馈问题和建议。

在克隆和构建代码库后，请查看[问题列表](https://github.com/lc-soft/LC-Finder/issues)。如果你是第一次使用 LCFinder，那么可以看看被标记为 [`good first issue`](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aissue+is%3Aopen+label%3A"good+first+issue") 的问题。

## 构建和运行

如果你想了解 LCFinder 的工作原理或想要调试某个问题，你将需要获取源代码，构建它，并在本地运行工具。

### 获取源代码

    git clone https://github.com/lc-soft/LC-Finder.git


### 先决条件

- [Git](https://git-scm.com/)
- [NodeJS](https://nodejs.org/)
- C/C++ 编译工具链
  - **Windows** 
    - [Visual Stuido 2017 社区版](https://visualstudio.microsoft.com/downloads/)，确保已经选择安装全部 C++ 工具和 Windows SDK.
  - **Linux**
    - `make`
    - [GCC](https://gcc.gnu.org/) 或者其它编译工具链
- 依赖库
  - [LCUI](https://github.com/lc-soft/LCUI)
  - [LCUI.css](https://github.com/lc-ui/lcui.css)
  - [sqlite3](https://www.sqlite.org/)
  - [unqlite](https://www.unqlite.org/)
  - [libyaml](https://github.com/yaml/libyaml)

最后，运行如下命令安装全部依赖：

    cd LC-Finder
    ./setup.sh

`setup.sh` 脚本会下载在构建 LCFinder 时所需的工具以及相关依赖项，只需要运行一次即可。

如果是在 Windows 系统下，建议使用 [vcpkg](https://github.com/Microsoft/vcpkg) 安装依赖，命令如下：

    .\vcpkg install libyaml sqlite3 leveldb

## 构建

### Windows

使用 VisualStudio 打开 `LC-Finder.sln` 文件，然后在界面顶部的菜单栏中选择`生成 -> 生成解决方案`。

### Linux

运行以下命令：

    ./build.sh

## 运行

进入 app 目录，运行 LC-Finder 文件。

## 目录结构

``` text
UWP              针对 UWP 平台的相关源代码及文件
app              应用程序目录
  assets         应用程序资源文件
    views        视图描述文件
    stylesheets  界面样式
    fonts        字体文件
  locales        用于本地化的语言翻译文件
config           相关配置
include          头文件
src              源代码
  lib            基础功能库
  scss           SCSS 文件，包含用户界面相关的样式
    common       通用样式
    componets    组件样式
    views        视图样式
  ui             用户界面
    views        视图控制器
    components   界面组件
vendor           第三方依赖库
```

## 拉取请求

为了使我们能够快速审查并接受你的拉取请求，请始终为每个问题创建一个拉取请求，并在拉取请求中链接该问题。除非具有相同的根本原因，否则切勿将多个请求合并成一个。请务必遵循我们的编码规范，并尽可能让代码的改动最小化。拉取请求应尽可能包含测试用例。

## 从哪开始贡献

在[完整的问题列表](https://github.com/lc-soft/LC-Finder/issues)中查看所有可贡献的地方，注意，即便有问题出现在问题列表里，也并不会意味着我们可以接受任何拉取请求，有几个原因我们可能不会接受拉取请求，例如：

- 用户体验 - 如果你对现有的图形界面有所不满，比如不喜欢界面风格、操作不方便等，可以先反馈问题，提供大致的改进方案和有说服力的理由，让我们确定是否需要改动。
- 架构 - 如果你的改动涉及到架构，则需要先征得作者同意。

为了增加拉取请求被合并的机会，你应该选择被 [`help-wanted`](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) 或 [`bug`](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aopen+is%3Aissue+label%3A%22bug%22) 标记的问题。如果你要处理的问题没有被这些标签标记，你可以与问题创建者交流，询问是否会考虑外部贡献。

## 翻译

我们使用 yaml 格式的文件存放翻译文本，它们位于 `app/locales` 目录下，如果你想添加其它语言的翻译，可以参考 `en-us.yaml` 的内容添加翻译文本。
