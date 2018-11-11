<p align="center">
  <a href="http://lcfinder.lc-soft.io/">
    <img src="https://lcfinder.lc-soft.io/static/images/logo-lcfinder.png" alt="" width=72 height=72>
  </a>
  <h3 align="center">LC's Finder</h3>
  <p align="center">
    轻量、跨平台的图片资源管理器
  </p>
  <p align="center">
    <a href="https://opensource.org/licenses/GPL-2.0"><img src="https://img.shields.io/github/license/lc-soft/LC-Finder.svg" alt="License"></a>
    <a href="https://github.com/lc-soft/LCUI/releases"><img src="https://img.shields.io/github/release/lc-soft/LC-Finder/all.svg" alt="Github Release"></a>
    <a href="https://github.com/lc-soft/LCUI/releases"><img src="https://img.shields.io/github/downloads/lc-soft/LC-Finder/total.svg" alt="Github All Releases"></a>
    <img src="https://img.shields.io/github/repo-size/lc-soft/LC-Finder.svg" alt="Repo size">
    <img src="https://img.shields.io/github/languages/code-size/lc-soft/LC-Finder.svg" alt="Code size">
  </p>
</p>

## 介绍

LCFinder (LC's Finder) 是一个图片资源管理工具，你可以用它来浏览图片、对图片进行简单的管理，或者搜索你想要的图片。和作者的其它项目一样，命名方式很简单，以 LC 开头，后面的 Finder 参考自 Mac OS 中的 Finder。

LCFinder 的界面及功能设计参考了 Windows 系统自带的“照片”应用，虽说是参考，但功能方面主要是按作者的个人需求而开发的，而且作者并不打算浪费时间去实现“照片”应用全部功能。“照片”应用在硬件配置一般的系统里会有明显的卡顿现象，尤其是在资源管理器里打开图片文件时，窗口会长时间处于未响应的状态，使用体验很差，容易让人怀疑：从打开文件解码数据，到构建界面并呈现图片，就这么点操作也需要耗费这么长时间？因此，这也是 LCFinder 的开发原因之一。

### 功能及特性

- **集锦视图：** 按时间分组，展示所有图片
- **文件夹视图：** 保留文件夹结构，可以单独查看各个文件夹里的图片
- **图片查看器：** 支持打开 bmp、jpg、png 格式的图片，能够对当前图片评分和添加标签
- **标签搜索：** 支持按标签来搜索匹配的图片
- **多语言支持：** 自带简体中文、繁体中文、英文翻译文件，可扩展支持其它语言
- **私人空间：** 将非公开的图片放入私人空间里可以隐藏它们，只有输入密码并确认开启私人空间后才会显示它们
- **UWP：** 支持 Windows 通用应用平台（UWP）

### 缺少的功能

- **图片编辑：** 需要有裁剪、旋转、缩放、马赛克、滤镜等基本的编辑功能
- **GIF 动图：** 支持播放 gif 动图
- **智能整理：** 让用户手动为众多图片添加标签是一件麻烦事，要是能够识别图片内容并自动分类整理图片，那将为用户省去很多不必要的时间浪费
- **颜色搜索:** 有的时候需要找一张图片，但不知道具体内容是什么，只知道它大致包含的颜色，这时如果有颜色搜索功能的话，就能从颜色相近的图片中找了

## 截图

[![screenshot 1](https://lcfinder.lc-soft.io/static/images/screenshot-001.jpg "效果图")](https://lcfinder.lc-soft.io/static/images/screenshot-001.jpg)
[![screenshot 1](https://lcfinder.lc-soft.io/static/images/screenshot-004.jpg "效果图")](https://lcfinder.lc-soft.io/static/images/screenshot-004.jpg)

## 贡献

在上面的[介绍](#介绍)中有提到缺少的功能，如果你有兴趣帮助解决问题并直接贡献到代码库，请先阅读[贡献指南](CONTRIBUTING.md)，其中包括以下内容：

- [如何构建并运行](CONTRIBUTING.md#构建和运行)
- [目录结构](CONTRIBUTING.md#目录结构)
- [提交合并请求](CONTRIBUTING.md#拉取请求)
- [贡献翻译](CONTRIBUTING.md#翻译)

## 反馈

- 在 [GitHub](https://github.com/lc-soft/LC-Finder/issues) 上请求添加新功能
- 在 [GitHub Issues](https://github.com/lc-soft/LC-Finder/issues) 页面里提交 bug
- 为[受欢迎的功能需求](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aopen+is%3Aissue+label%3Afeature-request+sort%3Areactions-%2B1-desc)投票

## 相关项目

LCFinder 的基础功能都离不开这些项目的支持：

- [LCUI](https://lcui.lc-soft.io) — 图形界面引擎，提供图形界面支持
- [LCUI.css](https://github.com/lc-ui/lcui.css) — UI 的组件库，为图形界面提供基础的样式和组件

## 许可证

遵循 [GPL 许可证](https://opensource.org/licenses/GPL-2.0)发布。
