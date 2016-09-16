# LC-Finder

[![Join the chat at https://gitter.im/lc-soft/LC-Finder](https://badges.gitter.im/lc-soft/LC-Finder.svg)](https://gitter.im/lc-soft/LC-Finder?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

一个简单的图片资源检索与管理工具，源代码使用 C 语言编写，基于 [GNU通用公共许可协议](http://www.gnu.org/licenses/gpl-2.0.html) 
发布。

## 特性
- 缩略图预览
- 文件夹浏览
- 标签搜索
- 支持触控

## 构建

### Windows

使用 VisualStudio 打开 `LC-Finder.sln` 文件，然后在界面顶部的菜单栏中选择 生成 -> 生成解决方案。

在成功生成后，将 bin 目录下 `LC-Finder.exe` 文件复制到根目录并运行它。

### Linux

运行以下命令：

	./autogen.sh
	./build.sh

`autogen.sh` 脚本会下载在构建 LC-Finder 时所需的工具以及相关依赖项，只需要运行一次即可。

在成功生成后，可以直接输入 `build/lcfinder` 命令行来运行 LC-Finder。

### 依赖项

以下依赖项都是必需的。

 * [LCUI](https://lcui.lc-soft.io) — 作者写的一个图形界面引擎，为本程序提供图形界面支持。
 * [sqlite3](https://www.sqlite.org/) — 轻量级的关系型数据库引擎，为文件信息索引与搜索功能提供支持。
 * [unqlite](https://www.unqlite.org/) — 嵌入式的非关系型数据引擎，为缩略图缓存功能提供支持。
 
通常 Github 上的 Releases 页面中会提供包含这些依赖库及头文件的压缩包，因此你不用再手动去编译这些依赖库。

## 截图

由于图片占用的页面面积太大，所以放在文档末尾了。

[![](https://cloud.githubusercontent.com/assets/1730073/16550438/0dc2374c-41dd-11e6-85be-b7103df79a7b.png "效果图")](https://cloud.githubusercontent.com/assets/1730073/16550438/0dc2374c-41dd-11e6-85be-b7103df79a7b.png)
[![](https://cloud.githubusercontent.com/assets/1730073/16550610/2d4350e0-41df-11e6-980c-061a0ea160b5.png "效果图")](https://cloud.githubusercontent.com/assets/1730073/16550610/2d4350e0-41df-11e6-980c-061a0ea160b5.png)
[![](https://cloud.githubusercontent.com/assets/1730073/15278979/e7943edc-1b51-11e6-87ab-7fc673b978b2.png "效果图")](https://cloud.githubusercontent.com/assets/1730073/15278979/e7943edc-1b51-11e6-87ab-7fc673b978b2.png)
