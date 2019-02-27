<p align="center">
  <a href="http://lcfinder.lc-soft.io/">
    <img src="https://lcfinder.lc-soft.io/static/images/logo-lcfinder.png" alt="" width=72 height=72>
  </a>
  <h3 align="center">LC's Finder</h3>
  <p align="center">
    图像标注与目标检测工具
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

LCFinder (LC's Finder) 是一个支持图像标注与目标检测的图片管理工具。和作者的其它项目一样，命名方式很简单，以 LC 开头，后面的 Finder 参考自 Mac OS 中的 Finder。

LCFinder 的界面及功能设计参考了 Windows 系统自带的“照片”应用，虽说是参考，但功能方面主要是按作者的个人需求而开发的，而且作者并不打算浪费时间去实现“照片”应用全部功能。

### 功能特性

- **图像标注：** 支持标注图片中的对象的边界框，标注数据保存在 txt 文件中，可用于训练 Yolo v3 和 v2 网络模型
- **目标检测：** 内置一个基于 darknet 的检测器，支持使用现有的预训练模型检测图片，检测到目标后会自动标注边界框并添加标签
- **图片查看器：** 支持打开 bmp、jpg、png 格式的图片
- **标签搜索：** 支持按标签来搜索图片，以便浏览已分类的图片
- **多语言支持：** 自带简体中文、繁体中文、英文翻译文件，可扩展支持其它语言
- **私人空间：** 带有密码保护的空间，将非公开的图片源放到此空间内可以隐藏它们
- **UWP：** 支持 Windows 通用应用平台（UWP），你可以[点击此链接](https://www.microsoft.com/store/apps/9NBLGGH401X5)到微软应用商店中查看它。注意，受限于 UWP 版本的开发成本，并不会与桌面版同步更新。

### 缺少的功能

- **训练模型：** 现在还不能训练新的模型，如果你熟悉 darknet，可以帮助我们完善 [darknetlib](https://github.com/lc-soft/darknetlib)

## 截图

[![screenshot 1](screenshots/1.jpg "LCFinder")](screenshots/1.jpg)

[![screenshot 1](screenshots/2.jpg "LCFinder")](screenshots/2.jpg)

[![screenshot 1](screenshots/3.jpg "LCFinder")](screenshots/3.jpg)

[![screenshot 1](screenshots/4.jpg "LCFinder")](screenshots/4.jpg)

## 贡献

在上面的[介绍](#介绍)中有提到缺少的功能，如果你有兴趣帮助解决问题并直接贡献到代码库，请先阅读[贡献指南](CONTRIBUTING.md)，其中包括以下内容：

- [如何构建并运行](CONTRIBUTING.md#构建和运行)
- [目录结构](CONTRIBUTING.md#目录结构)
- [提交合并请求](CONTRIBUTING.md#拉取请求)
- [贡献翻译](CONTRIBUTING.md#翻译)

## 相关项目

LCFinder 的基础功能都离不开这些项目的支持：

- [LCUI](https://lcui.lc-soft.io) — 图形界面引擎，提供图形界面支持
- [LCUI.css](https://github.com/lc-ui/lcui.css) — UI 的组件库，为图形界面提供基础的样式和组件
- [darknetlib](https://github.com/lc-soft/darknetlib) — darknet 的 C API 库，提供图像识别功能

## 许可证

遵循 [GPL 许可证](https://opensource.org/licenses/GPL-2.0)发布，仅供技术交流和学习之用。
