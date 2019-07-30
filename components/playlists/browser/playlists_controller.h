/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_CONTROLLER_H_
#define BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "brave/components/playlists/browser/playlists_media_file_controller.h"
#include "brave/components/playlists/browser/playlists_types.h"


namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // network

class PlaylistsControllerObserver;
class PlaylistsDBController;

class PlaylistsController : PlaylistsMediaFileController::Client {
 public:
  explicit PlaylistsController(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PlaylistsController() override;

  bool initialized() const { return initialized_; }

  void Init(const base::FilePath& base_dir);

  // False when |params| are not sufficient for new playlist.
  bool CreatePlaylist(const CreatePlaylistParams& params);
  void GetAllPlaylists();
  void GetPlaylist(const std::string& id);
  void DeletePlaylist(const std::string& id);
  void DeleteAllPlaylists();

  void AddObserver(PlaylistsControllerObserver* observer);
  void RemoveObserver(PlaylistsControllerObserver* observer);

 private:
  // PlaylistsMediaFileController::Client overrides:
  void OnMediaFileReady(base::Value&& playlist_value) override;
  void OnMediaFileGenerationFailed(base::Value&& playlist_value) override;

  // See below comments about step 1,2.
  void PutInitialPlaylistToDB(const std::string& key,
                              const std::string& value,
                              base::Value&& playlist_value);
  void OnPutInitialPlaylist(base::Value&& playlist_value, bool result);

  void DownloadThumbnail(base::Value&& playlist_value,
                         bool directory_ready);
  void OnThumbnailDownloaded(base::Value&& playlist_value,
                             network::SimpleURLLoader* loader,
                             base::FilePath path);
  void PutThumbnailReadyPlaylistToDB(const std::string& key,
                                     const std::string& json_value,
                                     base::Value&& playlist_value);
  void OnPutThumbnailReadyPlaylist(base::Value&& playlist_value,
                                   bool result);
  void PutPlayReadyPlaylistToDB(const std::string& key,
                                const std::string& json_value,
                                base::Value&& playlist_value);
  void OnPutPlayReadyPlaylist(base::Value&& playlist_value, bool result);
  void PutAbortedPlaylistToDB(const std::string& key,
                                const std::string& json_value,
                                base::Value&& playlist_value);
  void OnPutAbortedPlaylist(base::Value&& playlist_value, bool result);
  void GenerateMediaFile();

  void OnGetPlaylist(const std::string& value);

  base::SequencedTaskRunner* io_task_runner();

  // Clean up temporarily downloaded files and they are useless.
  void CleanUp();

  void OnDBInitialized(bool initialized);

  void NotifyPlaylistChanged(const PlaylistsChangeParams& params);

  bool initialized_ = false;
  base::FilePath base_dir_;

  // Playlist creation can be ready to play via below three steps.
  // Step 0. When creation is requested, requested info is put to db and
  //         notification is delivered to user with basic infos like playlist
  //         name and titles if provided. playlist is visible to user but it
  //         doesn't have thumbnail.
  // Step 1. Getting basic infos for showing this playlist to users. Currently
  //         it is only thumbnail image for this playlist.
  //         When thumbnail is fetched, it goes to step 2 and notifying to user
  //         about this playlist has thumbnail. But, still not ready to play.
  // Step 2. Getting media files and combining them as a single media file.
  //         Then, user can get notification about this playlist is ready to
  //         play.

  base::queue<base::Value> pending_media_file_creation_jobs_;

  base::ObserverList<PlaylistsControllerObserver> observers_;

  std::unique_ptr<PlaylistsDBController> db_controller_;
  std::unique_ptr<PlaylistsMediaFileController> media_file_controller_;

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::flat_set<network::SimpleURLLoader*> url_loaders_;

  base::WeakPtrFactory<PlaylistsController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlaylistsController);
};

#endif  // BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_CONTROLLER_H_
