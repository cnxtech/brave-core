/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/playlists/browser/playlists_media_file_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "brave/components/playlists/browser/playlists_types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

constexpr unsigned int kRetriesCountOnNetworkChange = 1;
const base::FilePath::StringType kSourceMediaFilesDir(
    FILE_PATH_LITERAL("source_files"));

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTagForURLLoad() {
  return net::DefineNetworkTrafficAnnotation("playlists_controller", R"(
      semantics {
        sender: "Brave Playlists Controller"
        description:
          "Fetching thumbnail image for newly created playlist"
        trigger:
          "User-initiated for creating new playlists "
        data:
          "Thumbnail for playlist"
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
      })");
}

void PrintValue(const base::Value& value) {
  std::string output;
  base::JSONWriter::Write(value, &output);
  LOG(ERROR) << __func__ << output;
}

bool EnsureDirectoryExists(const base::FilePath& path) {
  if (base::DirectoryExists(path))
    return true;

  return base::CreateDirectory(path);
}

}  // namespace

PlaylistsMediaFileController::PlaylistsMediaFileController(
    Client* client,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::FilePath& base_dir)
    : client_(client),
      url_loader_factory_(url_loader_factory),
      base_dir_(base_dir),
      weak_factory_(this) {
}

PlaylistsMediaFileController::~PlaylistsMediaFileController() {
  for (auto* const loader : url_loaders_)
    delete loader;
  url_loaders_.clear();
}

void PlaylistsMediaFileController::NotifyFail() {
  ResetDownloadStatus();
  client_->OnMediaFileGenerationFailed(std::move(current_playlist_));
}

void PlaylistsMediaFileController::NotifySucceed() {
  ResetDownloadStatus();
  client_->OnMediaFileReady(std::move(current_playlist_));
}

void PlaylistsMediaFileController::GenerateSingleMediaFile(
    base::Value&& playlist_value) {
  DCHECK(!in_progress_);
  in_progress_ = true;
  current_playlist_ = std::move(playlist_value);
  current_playlist_id_ = *current_playlist_.FindStringKey("id");

  // Create PROFILE_DIR/playlists/ID/source_files dir to store each media files.
  // Then, downloads them in that directory.
  CreateSourceFilesDirThenDownloads();
}

void PlaylistsMediaFileController::CreateSourceFilesDirThenDownloads() {
  const base::FilePath source_files_dir =
      base_dir_.Append(current_playlist_id_).Append(kSourceMediaFilesDir);
  base::PostTaskAndReplyWithResult(
      io_task_runner(),
      FROM_HERE,
      base::BindOnce(&EnsureDirectoryExists, source_files_dir),
      base::BindOnce(&PlaylistsMediaFileController::OnSourceFilesDirCreated,
                     weak_factory_.GetWeakPtr()));
}

void PlaylistsMediaFileController::OnSourceFilesDirCreated(bool success) {
  if (!success) {
    NotifyFail();
    return;
  }

  DownloadAllMediaFileSources();
}

void PlaylistsMediaFileController::DownloadAllMediaFileSources() {
  base::Value* media_files =
      current_playlist_.FindPath("createParams.mediaFiles");
  PrintValue(*media_files);
  const int media_files_num = media_files->GetList().size();
  remained_download_files_ = media_files_num;
  for (int i = 0; i < media_files_num; ++i) {
    const std::string* url =
        media_files->GetList()[i].FindStringKey("mediaFileUrl");
    if (url) {
      DownloadMediaFile(GURL(*url), i);
    } else {
       NOTREACHED() << "Playlists has Empty media file url";
       NotifyFail();
       break;
    }
  }
}

void PlaylistsMediaFileController::DownloadMediaFile(const GURL& url,
                                                     int index) {
  LOG(ERROR) << __func__ << ": " << url.spec() << " at: " << index;

#if defined(OS_WIN)
  base::string16 file_name = base::NumberToString16(index);
#else
  std::string file_name = base::NumberToString(index);
#endif
  const base::FilePath file_path =
      base_dir_.Append(current_playlist_id_).Append(kSourceMediaFilesDir).
          Append(file_name);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(url);
  request->allow_credentials = false;
  network::SimpleURLLoader* loader = network::SimpleURLLoader::Create(
      std::move(request),
      GetNetworkTrafficAnnotationTagForURLLoad()).release();
  loader->SetAllowHttpErrorResults(true);
  url_loaders_.insert(loader);
  loader->SetRetryOptions(
      kRetriesCountOnNetworkChange,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  loader->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&PlaylistsMediaFileController::OnMediaFileDownloaded,
                     base::Unretained(this),
                     loader,
                     index),
      file_path);
}

void PlaylistsMediaFileController::OnMediaFileDownloaded(
    network::SimpleURLLoader* loader,
    int index,
    base::FilePath path) {
  DCHECK(url_loaders_.find(loader) != url_loaders_.end());
  url_loaders_.erase(loader);
  std::unique_ptr<network::SimpleURLLoader> scoped_loader(loader);

  if (path.empty()) {
    // TODO(simonhong): Handle this.
    LOG(ERROR) << __func__ << ": failed to download media file at " << index;
  }

  remained_download_files_--;

  if (IsDownloadFinished())
    DoGenerateSingleMediaFile();
}

void PlaylistsMediaFileController::DoGenerateSingleMediaFile() {
  NotifySucceed();
}

base::SequencedTaskRunner* PlaylistsMediaFileController::io_task_runner() {
  if (!io_task_runner_) {
    io_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        { base::MayBlock(),
          base::TaskPriority::BEST_EFFORT,
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN });
  }
  return io_task_runner_.get();
}

void PlaylistsMediaFileController::ResetDownloadStatus() {
  in_progress_ = false;

  for (auto* const loader : url_loaders_)
    delete loader;
  url_loaders_.clear();
}

bool PlaylistsMediaFileController::IsDownloadFinished() {
  return remained_download_files_ == 0;
}
