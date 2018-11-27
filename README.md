<p align="center">
  <a href="http://lcfinder.lc-soft.io/">
    <img src="https://lcfinder.lc-soft.io/static/images/logo-lcfinder.png" alt="" width=72 height=72>
  </a>
  <h3 align="center">LC's Finder</h3>
  <p align="center">
    Lightweight, cross-platform image manager
  </p>
  <p align="center">
    <a href="https://opensource.org/licenses/GPL-2.0"><img src="https://img.shields.io/github/license/lc-soft/LC-Finder.svg" alt="License"></a>
    <a href="https://github.com/lc-soft/LCUI/releases"><img src="https://img.shields.io/github/release/lc-soft/LC-Finder/all.svg" alt="Github Release"></a>
    <a href="https://github.com/lc-soft/LCUI/releases"><img src="https://img.shields.io/github/downloads/lc-soft/LC-Finder/total.svg" alt="Github All Releases"></a>
    <img src="https://img.shields.io/github/repo-size/lc-soft/LC-Finder.svg" alt="Repo size">
    <img src="https://img.shields.io/github/languages/code-size/lc-soft/LC-Finder.svg" alt="Code size">
  </p>
</p>

## Introduction

[中文版](README.zh-cn.md)

LCFinder (LC's Finder) is an image resource management tool that you can use to browse images, easily manage images, or search for images you want. As with the author's other projects, the naming is simple, it begins with LC, and the following Finder is referenced from the Finder in Mac OS.

LCFinder's user interface and feature design is based on the "photos" application that comes with Windows, Although it is a reference, the functional aspects are mainly developed according to the author's individual needs, and the author does not intend to waste time to implement all the functions of the "photo" application.

The "Photos" app responds slowly on machines with low hardware configurations, especially when opening an image file in the Explorer, the window will be in an unresponsive state for a long time, and the experience is very poor. It is easy to wonder: from the open file decoding data, to build user interface and render the image, to such a point operations also need to spend such a long time? Therefore, this is one of the reasons for the development of LCFinder.

### Features

- **Collection view:** Group by time to show all images.
- **Folders view:** Keep the folder structure, you can view the images in each folder separately.
- **Image view:** Support for opening bmp, jpg, and png images to score and tag current image.
- **Tag search:** Support searching for images by tag.
- **Localization:** Support Simplified Chinese, Traditional Chinese, English, expandable support for other languages.
- **Private space:** Put non-public images in a private space to hide them, only display them after entering your password and confirming that you have opened your private space.
- **UWP：** Support for Windows Universal Platform (UWP), you can [click this link](https://www.microsoft.com/store/apps/9NBLGGH401X5) to view it in the windows app store.

## Screenshots

[![screenshot 1](https://lcfinder.lc-soft.io/static/images/screenshot-001.jpg "效果图")](https://lcfinder.lc-soft.io/static/images/screenshot-001.jpg)
[![screenshot 2](https://lcfinder.lc-soft.io/static/images/screenshot-004.jpg "效果图")](https://lcfinder.lc-soft.io/static/images/screenshot-004.jpg)

## Contributing

If you are interested in fixing issues and contributing directly to the code base, please see the [contributing guidelines](CONTRIBUTING.md), which covers the following:

- [How to build and run from source](CONTRIBUTING.md#build-and-run)
- [Project Structure](CONTRIBUTING.md#project-structure)
- [Submitting pull requests](CONTRIBUTING.md#pull-requests)
- [Contributing to translations](CONTRIBUTING.md#translations)

## Feedback

- Request a new feature on [GitHub](https://github.com/lc-soft/LC-Finder/issues).
- File a bug in [GitHub Issues](https://github.com/lc-soft/LC-Finder/issues).
- Vote for [Popular Feature Requests](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aopen+is%3Aissue+label%3Afeature-request+sort%3Areactions-%2B1-desc).

## Related Projects

The development of LCFinder is inseparable from the support of these projects:

- [LCUI](https://lcui.lc-soft.io) — UI engine, provide graphical user interface support
- [LCUI.css](https://github.com/lc-ui/lcui.css) — UI component library, provides basic styles and components for the graphical user interface.

## License

Licensed under the  [GPL License](https://opensource.org/licenses/GPL-2.0).
