FDVideoPlayer - 基于 Qt + FFmpeg + SDL3 的跨平台视频播放器
============================================================

功能特性：
  - 支持常见视频格式：mp4, avi, mkv, mov, flv, wmv, rmvb, 3gp
  - 使用 FFmpeg 8.0.1 进行音视频解码
  - 使用 SDL3 进行音频输出
  - 音视频同步播放（以音频时钟为基准）
  - 支持播放/暂停/停止控制
  - 进度条拖拽预览

开发环境：
  - Qt 5.12+（需要 multimedia 模块）
  - FFmpeg 8.0.1（共享库版本）
  - SDL3
  - 编译器：MSVC 或 MinGW（支持 C++11）

目录结构：
  FDVideoPlayer/
  ├── FDVideoPlayer.pro    # Qt 工程文件
  ├── main.cpp              # 程序入口
  ├── mainwindow.h/cpp      # 主窗口（UI控制）
  ├── mainwindow.ui         # Qt UI 布局
  ├── video_thread.h/cpp    # 解码播放线程（FFmpeg + SDL3）
  ├── video_widget.h/cpp    # 视频渲染控件
  ├── picture/wz3.ico       # 程序图标
  ├── ffmpeg-8.0.1-full_build-shared/  # FFmpeg 8.0.1 共享库（需自行下载）
  └── SDL3/                 # SDL3 开发库

构建步骤：
  1. 下载 FFmpeg 8.0.1 共享库版本
     https://www.gyan.dev/ffmpeg/builds/
     解压到项目目录下的 ffmpeg-8.0.1-full_build-shared/

  2. 安装 Qt 5.12+（勾选 multimedia 模块）

  3. 用 Qt Creator 打开 FDVideoPlayer.pro，编译运行

注意事项：
  - 运行时需将 FFmpeg bin 目录下的所有 .dll 复制到 exe 所在目录
  - SDL3.dll 同理需复制到 exe 目录
  - FFmpeg 共享库为 GPL v3 协议，请注意开源合规
  - SDL3 库使用 Git LFS 管理

作者：junxuezheng
GitHub：https://github.com/junxuezheng
