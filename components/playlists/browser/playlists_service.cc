/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/playlists/browser/playlists_service.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "brave/common/extensions/api/brave_playlists.h"
#include "brave/components/playlists/browser/playlists_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"

namespace {

const base::FilePath::StringType kBaseDirName(FILE_PATH_LITERAL("playlists"));

extensions::EventRouter* GetEventRouter(Profile* profile) {
  return extensions::EventRouter::Get(profile);
}

std::string ConvertPlaylistsChangeType(PlaylistsChangeParams::ChangeType type) {
  switch (type) {
    case PlaylistsChangeParams::ChangeType::CHANGE_TYPE_ADDED:
      return "added";
    case PlaylistsChangeParams::ChangeType::CHANGE_TYPE_DELETED:
      return "deleted";
    case PlaylistsChangeParams::ChangeType::CHANGE_TYPE_ABORTED:
      return "aborted";
    case PlaylistsChangeParams::ChangeType::CHANGE_TYPE_THUMBNAIL_READY:
      return "thumbnail_ready";
    case PlaylistsChangeParams::ChangeType::CHANGE_TYPE_PLAY_READY:
      return "play_ready";
    default:
      NOTREACHED();
      return "unknown";
  }
}

bool EnsurePlaylistsBaseDirectoryExists(const base::FilePath& path) {
  if (base::DirectoryExists(path))
    return true;

  return base::CreateDirectory(path);
}

}  // namespace

PlaylistsService::PlaylistsService(Profile* profile)
    : observer_(this),
      base_dir_(profile->GetPath().Append(kBaseDirName)),
      profile_(profile),
      weak_factory_(this) {
  controller_.reset(new PlaylistsController(
      content::BrowserContext::GetDefaultStoragePartition(profile_)->
          GetURLLoaderFactoryForBrowserProcess()));
  observer_.Add(controller_.get());
}

PlaylistsService::~PlaylistsService() {
}

void PlaylistsService::Init() {
  base::PostTaskAndReplyWithResult(
      file_task_runner(),
      FROM_HERE,
      base::BindOnce(&EnsurePlaylistsBaseDirectoryExists, base_dir_),
      base::BindOnce(&PlaylistsService::OnBaseDirectoryReady,
                     weak_factory_.GetWeakPtr()));
}

void PlaylistsService::OnBaseDirectoryReady(bool ready) {
  // If we can't create directory in profile dir, give up.
  if (!ready)
    return;

  controller_->Init(base_dir_);

  // Not used anymore from now.
  file_task_runner_.reset();
}

void PlaylistsService::OnPlaylistsInitialized() {
  auto event = std::make_unique<extensions::Event>(
      extensions::events::BRAVE_PLAYLISTS_ON_INITIALIZED,
      extensions::api::brave_playlists::OnInitialized::kEventName,
      extensions::api::brave_playlists::OnInitialized::Create(),
      profile_);

  GetEventRouter(profile_)->BroadcastEvent(std::move(event));
}

void PlaylistsService::OnPlaylistsChanged(
    const PlaylistsChangeParams& params) {
  auto event = std::make_unique<extensions::Event>(
      extensions::events::BRAVE_PLAYLISTS_ON_PLAYLISTS_CHANGED,
      extensions::api::brave_playlists::OnPlaylistsChanged::kEventName,
      extensions::api::brave_playlists::OnPlaylistsChanged::Create(
          ConvertPlaylistsChangeType(params.change_type), params.playlist_id),
      profile_);

  GetEventRouter(profile_)->BroadcastEvent(std::move(event));
}

base::SequencedTaskRunner* PlaylistsService::file_task_runner() {
  if (!file_task_runner_) {
    file_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
                            { base::MayBlock(),
                              base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN });
  }
  return file_task_runner_.get();
}
