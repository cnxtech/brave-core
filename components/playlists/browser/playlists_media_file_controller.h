/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_MEDIA_FILE_CONTROLLER_H_
#define BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_MEDIA_FILE_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  //namespace network

class GURL;

// Handle one Playlist at once.
class PlaylistsMediaFileController {
 public:
  class Client {
   public:
    // Called when target media file generation succeed.
    virtual void OnMediaFileReady(base::Value&& playlist_value) = 0;
    // Called when target media file generation failed.
    virtual void OnMediaFileGenerationFailed(base::Value&& playlist_value) = 0;

   protected:
    virtual ~Client() {}
  };

  PlaylistsMediaFileController(
      Client* client,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& base_dir);
  virtual ~PlaylistsMediaFileController();

  void GenerateSingleMediaFile(base::Value&& playlist_value);

  bool in_progress() const { return in_progress_; }

 private:
  void ResetDownloadStatus();
  void DownloadAllMediaFileSources();
  void DownloadMediaFile(const GURL& url, int index);
  void CreateSourceFilesDirThenDownloads();
  void OnSourceFilesDirCreated(bool success);
  void OnMediaFileDownloaded(network::SimpleURLLoader* loader,
                             int index,
                             base::FilePath path);
  void DoGenerateSingleMediaFile();

  // True when all source media files are downloaded.
  // If it's true, single media file will be generated.
  bool IsDownloadFinished();

  void NotifyFail();
  void NotifySucceed();

  base::SequencedTaskRunner* io_task_runner();

  Client* client_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::FilePath base_dir_;
  base::Value current_playlist_;
  std::string current_playlist_id_;
  int remained_download_files_ = 0;

  // true when this class is working for playlist now.
  bool in_progress_ = false;

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::flat_set<network::SimpleURLLoader*> url_loaders_;

  base::WeakPtrFactory<PlaylistsMediaFileController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlaylistsMediaFileController);
};

#endif  // BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_MEDIA_FILE_CONTROLLER_H_
