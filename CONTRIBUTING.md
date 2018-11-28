# Contributing to LCFinder

[中文版](CONTRIBUTING.zh-cn.md)

There are many ways to contribute to the LCFinder project: logging bugs, submitting pull requests, reporting issues, and creating suggestions.

After cloning and building the repo, check out the [issues list](https://github.com/lc-soft/LC-Finder/issues). Issues labeled [`good first issue`](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aissue+is%3Aopen+label%3A"good+first+issue") are good candidates to pick up if you are in the code for the first time. If you are contributing significant changes, please discuss with the assignee of the issue first before starting to work on the issue.

## Build and Run

If you want to understand how Code works or want to debug an issue, you'll want to get the source, build it, and run the tool locally.

### Getting the sources

    git clone https://github.com/lc-soft/LC-Finder.git

### Prerequisites

- [Git](https://git-scm.com/)
- [NodeJS](https://nodejs.org/)
- C/C++ compiler tool chain
  - **Windows** 
    - [Visual Stuido 2017 Community](https://visualstudio.microsoft.com/downloads/), Make sure you have chosen to install all C++ tools and the Windows SDK.
  - **Linux**
    - [xmake](https://github.com/tboox/xmake)
    - [GCC](https://gcc.gnu.org/) or other build toolchain
- Dependencies
  - [LCUI](https://github.com/lc-soft/LCUI)
  - [LCUI.css](https://github.com/lc-ui/lcui.css)
  - [sqlite3](https://www.sqlite.org/)
  - [unqlite](https://www.unqlite.org/) or [leveldb](https://github.com/google/leveldb)
  - [libyaml](https://github.com/yaml/libyaml)

Finally, install all dependencies using:

    cd LC-Finder
    ./setup.sh

The `setup.sh` script downloads the tools and related dependencies needed to build LCFinder and only needs to be run once.

If you are using Windows, we recommend that you use [vcpkg](https://github.com/Microsoft/vcpkg) to install dependencies, the command is as follows:

    .\vcpkg install libyaml sqlite3 leveldb

## Build

**Windows**

Use VS2017 to open `LC-Finder.sln` file and click `Build -> Build solution` in the menu bar.

**Linux**

Run script:

    ./build.sh

## Run

Enter the `app` directory and run the `lc-finder` file.

## Project Structure

``` text
UWP              Code for the Universal Windows Platform (UWP)
app              Application work environment related files
  assets         Assets
    views        View files (.xml files)
    stylesheets  Stylesheet files (.css files)
    fonts        Font files
  locales        Language translation files
config           Configuration files
include          Header files
src              Source files
  lib            Base library source code
  scss           SCSS files, used to describe the UI style
    common       Common style
    componets    Component style
    views        View style
  ui             UI related source code
    views        View controllor source code
    components   Components source code
vendor           Third-party dependencies
```

## Pull Requests

To enable us to quickly review and accept your pull requests, always create one pull request per issue and [link the issue in the pull request](https://blog.github.com/2011-10-12-introducing-issue-mentions/). Never merge multiple requests in one unless they have the same root cause. Be sure to follow our Coding Guidelines and keep code changes as small as possible. Avoid pure formatting changes to code that has not been modified otherwise. Pull requests should contain tests whenever possible.

## Where to Contribute

Check out the [full issues list](https://github.com/lc-soft/LC-Finder/issues) for a list of all potential areas for contributions. Note that just because an issue exists in the repository does not mean we will accept a contribution to the core editor for it. There are several reasons we may not accept a pull request like:

- User experience - If you are dissatisfied with the existing UI, such as not like UI style, inconvenient operation, etc., you can first feedback the problem and provide a improvement plan and persuasive reasons, let us determine whether it needs to be changed.
- Architectural - The team and/or feature owner needs to agree with any architectural impact a change may make. 

To improve the chances to get a pull request merged you should select an issue that is labelled with the [`help-wanted`](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) or [`bug`](https://github.com/lc-soft/LC-Finder/issues?q=is%3Aopen+is%3Aissue+label%3A%22bug%22) labels. If the issue you want to work on is not labelled with `help-wanted` or `bug`, you can start a conversation with the issue owner asking whether an external contribution will be considered.

## Translations

We use yaml format files to store translated texts. They are located in the `app/locales` directory. If you want to add translations in other languages, you can add translations by referring to the contents of `en-us.yaml`.
