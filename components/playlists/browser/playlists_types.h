/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_TYPES_H_
#define BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_TYPES_H_

#include <string>
#include <vector>

struct PlaylistsChangeParams {
  enum class ChangeType {
    CHANGE_TYPE_ADDED,            // New playlist is added but not ready state.
    CHANGE_TYPE_THUMBNAIL_READY,  // Newlay added playst is ready to show thumb.
    CHANGE_TYPE_PLAY_READY,       // Newlay added playst is ready to play.
    CHANGE_TYPE_DELETED,          // A playlist is deleted.
    CHANGE_TYPE_ABORTED,          // Aborted during the creation process.
  };

  PlaylistsChangeParams(ChangeType type, const std::string& id);
  ~PlaylistsChangeParams();

  ChangeType change_type;
  std::string playlist_id;
};

struct MediaFileInfo {
  MediaFileInfo(const std::string& url, const std::string& title);
  ~MediaFileInfo();

  std::string media_file_url;
  std::string media_file_title;
};

struct CreatePlaylistParams {
  CreatePlaylistParams();
  CreatePlaylistParams(const CreatePlaylistParams& rhs);
  ~CreatePlaylistParams();

  std::string playlist_thumbnail_url;
  std::string playlist_name;
  std::vector<MediaFileInfo> media_files;
};

struct PlaylistInfo {
  PlaylistInfo();
  PlaylistInfo(const PlaylistInfo& rhs);
  ~PlaylistInfo();

  std::string id;
  std::string playlist_name;
  std::string thumbnail_path;
  std::string media_file_path;
  bool ready;

  CreatePlaylistParams create_params;
};

#endif  // BRAVE_COMPONENTS_PLAYLISTS_BROWSER_PLAYLISTS_TYPES_H_
