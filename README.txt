FDVideoPlayer — Qt + FFmpeg + SDL3 Video Player
==================================================

[中文]
功能特性：
  - 支持常见视频格式：mp4, avi, mkv, mov, flv, wmv, rmvb, 3gp
  - 使用 FFmpeg 8.0.1 进行音视频解码
  - 使用 SDL3 进行音频输出
  - 音视频同步播放（以音频时钟为基准）
  - 支持播放 / 暂停 / 停止控制
  - 进度条拖拽预览

开发环境：
  - Qt 5.12+（需要 multimedia 模块）
  - FFmpeg 8.0.1（共享库版本，需自行下载）
  - SDL3
  - 编译器：MSVC 或 MinGW，支持 C++11

目录结构：
  FDVideoPlayer/
  ├── FDVideoPlayer.pro    # Qt 工程文件
  ├── main.cpp              # 程序入口
  ├── mainwindow.h/cpp      # 主窗口（UI 控制）
  ├── mainwindow.ui         # Qt UI 布局
  ├── video_thread.h/cpp    # 解码播放线程（FFmpeg + SDL3）
  ├── video_widget.h/cpp    # 视频渲染控件
  ├── picture/wz3.ico       # 程序图标
  ├── ffmpeg-8.0.1-full_build-shared/  # FFmpeg 库（需自行下载）
  └── SDL3/                 # SDL3 库

构建步骤：
  1. 下载 FFmpeg 8.0.1 共享库版本
     https://www.gyan.dev/ffmpeg/builds/
     解压到项目目录下的 ffmpeg-8.0.1-full_build-shared/
  2. 安装 Qt 5.12+（勾选 multimedia 模块）
  3. 用 Qt Creator 打开 FDVideoPlayer.pro，编译运行

注意事项：
  - 运行时需将 FFmpeg bin 目录下的 .dll 复制到 exe 所在目录
  - SDL3.dll 同理需复制到 exe 目录
  - FFmpeg 共享库为 GPL v3 协议，请注意开源合规
  - SDL3 库使用 Git LFS 管理

[English]
Features:
  - Supports common video formats: mp4, avi, mkv, mov, flv, wmv, rmvb, 3gp
  - FFmpeg 8.0.1 for audio/video decoding
  - SDL3 for audio output
  - Audio-video sync playback (audio clock based)
  - Play / Pause / Stop controls
  - Progress slider with drag preview

Development Environment:
  - Qt 5.12+ (with multimedia module)
  - FFmpeg 8.0.1 (shared build, download separately)
  - SDL3
  - Compiler: MSVC or MinGW with C++11 support

Directory Structure:
  FDVideoPlayer/
  ├── FDVideoPlayer.pro    # Qt project file
  ├── main.cpp              # Entry point
  ├── mainwindow.h/cpp      # Main window (UI controls)
  ├── mainwindow.ui         # Qt UI layout
  ├── video_thread.h/cpp    # Decode & playback thread (FFmpeg + SDL3)
  ├── video_widget.h/cpp    # Video rendering widget
  ├── picture/wz3.ico       # App icon
  ├── ffmpeg-8.0.1-full_build-shared/  # FFmpeg libs (download separately)
  └── SDL3/                 # SDL3 library

Build Steps:
  1. Download FFmpeg 8.0.1 shared build from https://www.gyan.dev/ffmpeg/builds/
     Extract to ffmpeg-8.0.1-full_build-shared/ under the project directory
  2. Install Qt 5.12+ (check the multimedia module)
  3. Open FDVideoPlayer.pro with Qt Creator, build and run

Notes:
  - Copy all .dll files from FFmpeg bin/ to the same folder as the .exe at runtime
  - Do the same for SDL3.dll
  - FFmpeg shared build is under GPL v3 — ensure open-source compliance
  - SDL3 library is managed with Git LFS

Author: junxuezheng
GitHub: https://github.com/junxuezheng
