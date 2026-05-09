#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include "video_widget.h"
#include "video_thread.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void onOpenFile();
  void onPlayPause();
  void onStop();
  void onSliderPressed();
  void onSliderReleased();
  void onSliderValueChanged(int value);
  void onFrameReceived(const QImage &image);
  void onDurationChanged(qint64 duration);
  void onPositionChanged(qint64 position);
  void onPlaybackFinished();
  void onError(const QString &err);

private:
  void setupUI();

  VideoWidget *m_videoWidget;
  QPushButton *m_openBtn;
  QPushButton *m_playPauseBtn;
  QPushButton *m_stopBtn;
  QSlider *m_slider;
  QLabel *m_timeLabel;

  VideoThread *m_thread;
  QString m_currentFile;
  bool m_isSliderPressed;
  qint64 m_duration;
  qint64 m_currentPosition;

private:
    Ui::MainWindow *ui;

    bool m_isStop;
};

#endif // MAINWINDOW_H
