# LC-Finder

[![Join the chat at https://gitter.im/lc-soft/LC-Finder](https://badges.gitter.im/lc-soft/LC-Finder.svg)](https://gitter.im/lc-soft/LC-Finder?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Total downloads](https://img.shields.io/github/downloads/lc-soft/LC-Finder/total.svg)](https://github.com/lc-soft/LC-Finder/releases)
[![License](https://img.shields.io/badge/license-GPLv2-blue.svg)](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

一个图形化的图片资源检索与管理工具，主要使用 C 语言编写，基于 [GNU通用公共许可协议](http://www.gnu.org/licenses/gpl-2.0.html) 
发布。

[![](https://lcfinder.lc-soft.io/static/images/screenshot-001.jpg "效果图")](https://lcfinder.lc-soft.io/static/images/screenshot-001.jpg)
[![](https://lcfinder.lc-soft.io/static/images/screenshot-004.jpg "效果图")](https://lcfinder.lc-soft.io/static/images/screenshot-004.jpg)

## 功能及特性

- 缩略图预览
- 文件夹浏览
- 图片查看器
- 标签搜索
- 触控支持
- 多语言支持
- 支持 Windows 通用应用平台（UWP）

## 目录结构

``` text
UWP              针对 UWP 平台的相关源代码及文件
app              应用程序目录
  assets         应用程序资源文件
    views        视图描述文件
    stylesheets  界面样式
  locales        用于本地化的语言翻译文件
config           相关配置
include          头文件
src              源代码
  lib            基础功能库
  scss           SCSS 文件，包含界面相关的样式
    common       通用样式
      iconfont   图标样式
    componets    组件样式
    themes       主题样式
    views        视图样式
  ui             界面
    views        视图控制器
    components   界面组件
vendor           第三方依赖库
```

## 构建

### Windows

使用 VisualStudio 打开 `LC-Finder.sln` 文件，然后在界面顶部的菜单栏中选择 生成 -> 生成解决方案，成功生成后运行 app 目录下 `LC-Finder.exe`。

### Linux

目前已经暂停对 Linux 的支持，如有需要可向 [LCUI](https://github.com/lc-soft/LCUI) 项目提供 Linux 端的支持方案。

运行以下命令：

	./setup.sh
	./build.sh

`setup.sh` 脚本会下载在构建 LC-Finder 时所需的工具以及相关依赖项，只需要运行一次即可。

在成功生成后，可以直接输入 `app/lc-finder` 命令行来运行本程序。

### 依赖项

以下依赖项都是必需的。

 * [LCUI](https://lcui.lc-soft.io) — 作者写的一个图形界面引擎，为本程序提供图形界面支持。
 * [sqlite3](https://www.sqlite.org/) — 轻量级的关系型数据库引擎，为文件信息索引与搜索功能提供支持。
 * [unqlite](https://www.unqlite.org/) — 嵌入式的非关系型数据引擎，为缩略图缓存功能提供支持。
 
通常 Github 上的 Releases 页面中会提供包含这些依赖库及头文件的压缩包，因此你不用再手动去编译这些依赖库。

### 待办事项

- [ ] 文档：更新自述文档，添加英文版
- [ ] 修复：导航栏中的一个按钮被隐藏后，布局未更新
- [ ] 修复：内存泄漏以及内存访问越界问题
- [ ] 修复：有时会出现图标未载入的情况
- [ ] 修复：缩略图视图下有一些图片未加载
- [ ] 修复：标签列表视图和日期列表视图没有滚动条
- [ ] 改进：改用 LevelDB 存储缩略图数据
- [ ] 添加：添加界面缩放设置
- [ ] 重构：处理老代码
