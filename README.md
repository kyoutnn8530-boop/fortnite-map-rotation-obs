# Fortnite Map Rotation for OBS

Fortniteクリエイティブ配信用のWindows 64-bitネイティブOBSプラグインです。

## できること

- OBS内の「マップ順番管理」ドックからマップ名・コードを手動追加
- YouTubeライブチャットの「マップ希望 マップ名 1234-5678-9012」を承認待ちへ自動取得
- OBS内で希望を確認し、「承認して追加」または「却下」
- 完了／未完了、削除、並べ替え、全リセット
- 現在のマップ、次のマップ、一覧、進捗を透明背景で配信画面に表示
- マップ一覧をOBS終了後も自動保存

## インストール

Windows x64インストーラー（`.exe`）を実行します。OBSを起動中の場合は、先に終了してください。

## OBSでの使い方

1. OBSを起動します。
2. `ドック` メニューから `マップ順番管理` を表示します。
3. ソース欄の `＋` → `Fortnite マップ順番` を追加します。
4. 管理画面へマップ名とマップコードを入力します。
5. 終了したマップを選び、`完了／未完了` を押します。

## YouTubeチャット連携

1. Google CloudでYouTube Data API v3を有効にし、APIキーを作成します。
2. OBSドックへAPIキーと配信URLを入力し、`チャット監視を開始`を押します。
3. 視聴者は `マップ希望 マップ名 1234-5678-9012` とコメントします。
4. 「承認待ち」で確認し、`承認して追加`を押します。

監視開始より前のコメントは追加されません。同じコメントIDと同じマップコードは重複追加されません。

映像ソースは1280×720の透明キャンバスです。OBS上で好きな位置・大きさに調整できます。

## 対応環境

- Windows 10 / 11（64-bit）
- OBS Studio 31.1系

## ビルド

公式OBSプラグインテンプレートをベースにしています。GitHub ActionsへpushするとWindows版が自動ビルドされ、`1.0.0`のようなタグを付けるとインストーラー付きドラフトリリースが生成されます。

## ライセンス

GPL-2.0

<!-- Upstream template documentation follows. -->

## Introduction

The plugin template is meant to be used as a starting point for OBS Studio plugin development. It includes:

* Boilerplate plugin source code
* A CMake project file
* GitHub Actions workflows and repository actions

## Supported Build Environments

| Platform  | Tool   |
|-----------|--------|
| Windows   | Visual Studio 17 2022 |
| macOS     | XCode 16.0 |
| Windows, macOS  | CMake 3.30.5 |
| Ubuntu 24.04 | CMake 3.28.3 |
| Ubuntu 24.04 | `ninja-build` |
| Ubuntu 24.04 | `pkg-config`
| Ubuntu 24.04 | `build-essential` |

## Quick Start

An absolute bare-bones [Quick Start Guide](https://github.com/obsproject/obs-plugintemplate/wiki/Quick-Start-Guide) is available in the wiki.

## Documentation

All documentation can be found in the [Plugin Template Wiki](https://github.com/obsproject/obs-plugintemplate/wiki).

Suggested reading to get up and running:

* [Getting started](https://github.com/obsproject/obs-plugintemplate/wiki/Getting-Started)
* [Build system requirements](https://github.com/obsproject/obs-plugintemplate/wiki/Build-System-Requirements)
* [Build system options](https://github.com/obsproject/obs-plugintemplate/wiki/CMake-Build-System-Options)

## GitHub Actions & CI

Default GitHub Actions workflows are available for the following repository actions:

* `push`: Run for commits or tags pushed to `master` or `main` branches.
* `pr-pull`: Run when a Pull Request has been pushed or synchronized.
* `dispatch`: Run when triggered by the workflow dispatch in GitHub's user interface.
* `build-project`: Builds the actual project and is triggered by other workflows.
* `check-format`: Checks CMake and plugin source code formatting and is triggered by other workflows.

The workflows make use of GitHub repository actions (contained in `.github/actions`) and build scripts (contained in `.github/scripts`) which are not needed for local development, but might need to be adjusted if additional/different steps are required to build the plugin.

### Retrieving build artifacts

Successful builds on GitHub Actions will produce build artifacts that can be downloaded for testing. These artifacts are commonly simple archives and will not contain package installers or installation programs.

### Building a Release

To create a release, an appropriately named tag needs to be pushed to the `main`/`master` branch using semantic versioning (e.g., `12.3.4`, `23.4.5-beta2`). A draft release will be created on the associated repository with generated installer packages or installation programs attached as release artifacts.

## Signing and Notarizing on macOS

Basic concepts of codesigning and notarization on macOS are explained in the correspodning [Wiki article](https://github.com/obsproject/obs-plugintemplate/wiki/Codesigning-On-macOS) which has a specific section for the [GitHub Actions setup](https://github.com/obsproject/obs-plugintemplate/wiki/Codesigning-On-macOS#setting-up-code-signing-for-github-actions).
