<p align="center">
  <a href="http://lcfinder.lc-soft.io/">
    <img src="https://lcfinder.lc-soft.io/static/images/logo-lcfinder.png" alt="" width=72 height=72>
  </a>
  <h3 align="center">LC's Finder</h3>
  <p align="center">
    Image annotation and object detection tool
  </p>
  <p align="center">
    <a class="https://github.com/lc-soft/LC-Finder/actions"><img src="https://github.com/lc-soft/LC-Finder/workflows/C%2FC%2B%2B%20CI/badge.svg" alt="GitHub Actions"></a>
    <a class="https://travis-ci.org/lc-soft/LC-Finder"><img src="https://travis-ci.org/lc-soft/LC-Finder.svg?branch=develop" alt="Build Status"></a>
    <a href="https://opensource.org/licenses/GPL-2.0"><img src="https://img.shields.io/github/license/lc-soft/LC-Finder.svg" alt="License"></a>
    <a href="https://github.com/lc-soft/LCUI/releases"><img src="https://img.shields.io/github/release/lc-soft/LC-Finder/all.svg" alt="Github Release"></a>
    <a href="https://github.com/lc-soft/LCUI/releases"><img src="https://img.shields.io/github/downloads/lc-soft/LC-Finder/total.svg" alt="Github All Releases"></a>
    <img src="https://img.shields.io/github/repo-size/lc-soft/LC-Finder.svg" alt="Repo size">
    <img src="https://img.shields.io/github/languages/code-size/lc-soft/LC-Finder.svg" alt="Code size">
  </p>
</p>

## Introduction

[中文版](README.zh-cn.md)

LCFinder (LC's Finder) is an image management tool that supports image annotation and object detection. It is written in C and uses [LCUI](https://github.com/lc-soft/LCUI) for its graphical interface. As with the author's other projects, the naming is simple, it begins with LC, and the following Finder is referenced from the Finder in Mac OS.

LCFinder's user interface and feature design is based on the "photos" application that comes with Windows, Although it is a reference, the functional aspects are mainly developed according to the author's individual needs, and the author does not intend to waste time to implement all the functions of the "photo" application.

### Features

- **Image annotation:** Provides a simple GUI for marking bounded boxes of objects in images for training Yolo v3 and v2
- **Object detection:** Built-in image detector, It can automatically annotate the detected objects in the images.
- **Search by tags:** You can browse and search tagged images in the tags view
- **Localization:** Support English, Simplified Chinese, Traditional Chinese, expandable support for other languages.
- **Private space:** A password-protected space where you can hide non-public image sources
- **UWP:** Support for Windows Universal Platform (UWP), you can [click this link](https://www.microsoft.com/store/apps/9NBLGGH401X5) to view it in the windows app store. Due to the development cost of the UWP version, it will not be updated with the desktop version.

## Screenshots

[![screenshot 1](screenshots/1.jpg "LCFinder")](screenshots/1.jpg)

[![screenshot 1](screenshots/2.jpg "LCFinder")](screenshots/2.jpg)

[![screenshot 1](screenshots/3.jpg "LCFinder")](screenshots/3.jpg)

[![screenshot 1](screenshots/4.jpg "LCFinder")](screenshots/4.jpg)

## Install

If you want to use the detector, you need the following steps:

1. Download pre-trained models:
    - `yolov3.cfg` (236 MB COCO Yolo v3) - requires 4 GB GPU-RAM: https://pjreddie.com/media/files/yolov3.weights
    - `yolov3-tiny.cfg` (34 MB COCO Yolo v3 tiny) - requires 1 GB GPU-RAM: https://pjreddie.com/media/files/yolov3-tiny.weights
    - `yolo9000.cfg` (186 MB Yolo9000-model) - requires 4 GB GPU-RAM: http://pjreddie.com/media/files/yolo9000.weights
1. Copy the `.weights` file to its namesake directory in the `app/detector/models` directory, such as: copy `yolov3.weights` file to `app/detector/models/yolov3/`

## Contributing

If you are interested in fixing issues and contributing directly to the code base, please see the [contributing guidelines](CONTRIBUTING.md), which covers the following:

- [How to build and run from source](CONTRIBUTING.md#build-and-run)
- [Project Structure](CONTRIBUTING.md#project-structure)
- [Submitting pull requests](CONTRIBUTING.md#pull-requests)
- [Contributing to translations](CONTRIBUTING.md#translations)

## Related Projects

The development of LCFinder is inseparable from the support of these projects:

- [LCUI](https://github.com/lc-soft/LCUI) — UI engine, provide graphical user interface support
- [LCUI.css](https://github.com/lc-ui/lcui.css) — UI component library, provides basic styles and components for the graphical user interface
- [darknetlib](https://github.com/lc-soft/darknetlib) — C bindings for [darknet](http://pjreddie.com/darknet/), provide image recognition support


## License

Licensed under the  [GPL License](https://opensource.org/licenses/GPL-2.0).
