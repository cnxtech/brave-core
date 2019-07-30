/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/playlists/browser/playlists_controller.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/token.h"
#include "brave/components/playlists/browser/playlists_controller_observer.h"
#include "brave/components/playlists/browser/playlists_db_controller.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

constexpr unsigned int kRetriesCountOnNetworkChange = 1;
const base::FilePath::StringType kDatabaseDirName(
    FILE_PATH_LITERAL("playlists_db"));
const base::FilePath::StringType kThumbnailFileName(
    FILE_PATH_LITERAL("thumbnail"));

void PrintValue(const base::Value& value) {
  std::string output;
  base::JSONWriter::Write(value, &output);
  LOG(ERROR) << __func__ << ": " << output;
}

PlaylistInfo CreatePlaylistInfo(const CreatePlaylistParams& params) {
  PlaylistInfo p;
  p.id = base::Token::CreateRandom().ToString();
  p.playlist_name = params.playlist_name;
  p.create_params = params;
  return p;
}

base::Value GetValueFromMediaFile(const MediaFileInfo& info) {
  base::Value media_file(base::Value::Type::DICTIONARY);
  media_file.SetStringKey("mediaFileUrl", info.media_file_url);
  media_file.SetStringKey("mediaFileTitle", info.media_file_title);
  return media_file;
}

base::Value GetValueFromMediaFiles(
    const std::vector<MediaFileInfo>& media_files) {
  base::Value media_files_value(base::Value::Type::LIST);
  for (const MediaFileInfo& info : media_files)
    media_files_value.GetList().push_back(GetValueFromMediaFile(info));
  return media_files_value;
}

base::Value GetValueFromCreateParams(const CreatePlaylistParams& params) {
  base::Value create_params_value(base::Value::Type::DICTIONARY);
  create_params_value.SetStringKey("playlistThumbnailUrl",
                                   params.playlist_thumbnail_url);
  create_params_value.SetStringKey("playlistName",
                                   params.playlist_name);
  create_params_value.SetKey("mediaFiles",
                             GetValueFromMediaFiles(params.media_files));
  return create_params_value;
}

base::Value GetTitleValueFromCreateParams(const CreatePlaylistParams& params) {
  base::Value titles_value(base::Value::Type::LIST);
  for (const MediaFileInfo& info : params.media_files)
    titles_value.GetList().emplace_back(info.media_file_title);
  return titles_value;
}

base::Value GetValueFromPlaylistInfo(const PlaylistInfo& info) {
  base::Value playlist_value(base::Value::Type::DICTIONARY);
  playlist_value.SetStringKey("id", info.id);
  playlist_value.SetStringKey("playlistName", info.playlist_name);
  playlist_value.SetStringKey("thumbnailPath", info.thumbnail_path);
  playlist_value.SetStringKey("mediaFilePath", info.media_file_path);
  playlist_value.SetKey("titles",
                        GetTitleValueFromCreateParams(info.create_params));
  playlist_value.SetKey("createParams",
                         GetValueFromCreateParams(info.create_params));
  return playlist_value;
}

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

bool EnsureDirectoryExists(const base::FilePath& path) {
  if (base::DirectoryExists(path))
    return true;

  return base::CreateDirectory(path);
}

}  // namespace

PlaylistsController::PlaylistsController(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      weak_factory_(this) {
}

PlaylistsController::~PlaylistsController() {
  for (auto* const loader : url_loaders_)
    delete loader;
  url_loaders_.clear();

  io_task_runner()->DeleteSoon(FROM_HERE, std::move(db_controller_));

}

void PlaylistsController::Init(const base::FilePath& base_dir) {
  base_dir_ = base_dir;
  db_controller_.reset(
      new PlaylistsDBController(base_dir.Append(kDatabaseDirName)));
  media_file_controller_.reset(
      new PlaylistsMediaFileController(this, url_loader_factory_, base_dir));

  base::PostTaskAndReplyWithResult(
      io_task_runner(),
      FROM_HERE,
      base::BindOnce(&PlaylistsDBController::Init,
                     base::Unretained(db_controller_.get())),
      base::BindOnce(&PlaylistsController::OnDBInitialized,
                     weak_factory_.GetWeakPtr()));
}

void PlaylistsController::OnDBInitialized(bool initialized) {
  initialized_ = initialized;
  LOG(ERROR) << __func__ << ": " << initialized_;

  if (!initialized_) {
    return;
  }

  for (PlaylistsControllerObserver& obs : observers_)
    obs.OnPlaylistsInitialized();
}

void PlaylistsController::NotifyPlaylistChanged(
    const PlaylistsChangeParams& params) {
  for (PlaylistsControllerObserver& obs : observers_)
    obs.OnPlaylistsChanged(params);
}

void PlaylistsController::DownloadThumbnail(base::Value&& playlist_value,
                                            bool directory_ready) {
  if (!directory_ready)
    return;

  base::Value value = std::move(playlist_value);
  const std::string* thumbnail_url =
      value.FindStringPath("createParams.playlistThumbnailUrl");

  if (!thumbnail_url || (*thumbnail_url).empty()) {
    // TODO(simonhong): If thumbnail url is not available skip downloading it
    // and goes step 2 directly.
    NOTREACHED() << " thumbnail url is not available";
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(*thumbnail_url);
  request->allow_credentials = false;
  network::SimpleURLLoader* loader = network::SimpleURLLoader::Create(
      std::move(request),
      GetNetworkTrafficAnnotationTagForURLLoad()).release();
  loader->SetAllowHttpErrorResults(true);
  url_loaders_.insert(loader);
  loader->SetRetryOptions(
      kRetriesCountOnNetworkChange,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  const std::string playlist_id = *value.FindStringKey("id");
  base::FilePath thumbnail_path =
      base_dir_.Append(playlist_id).Append(kThumbnailFileName);
  loader->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&PlaylistsController::OnThumbnailDownloaded,
                     base::Unretained(this),
                     std::move(value),
                     loader),
      thumbnail_path);
}

void PlaylistsController::OnThumbnailDownloaded(
    base::Value&& playlist_value,
    network::SimpleURLLoader* loader,
    base::FilePath path) {
  DCHECK(url_loaders_.find(loader) != url_loaders_.end());
  url_loaders_.erase(loader);
  std::unique_ptr<network::SimpleURLLoader> scoped_loader(loader);

  if (path.empty())
    return;

  const std::string thumbnail_path =
#if defined(OS_WIN)
      base::UTF16ToUTF8(path.value());
#else
      path.value();
#endif
  base::Value value(std::move(playlist_value));
  value.SetStringKey("thumbnailPath", thumbnail_path);

  std::string output;
  base::JSONWriter::Write(value, &output);
  const std::string playlist_id = *value.FindStringKey("id");
  PutThumbnailReadyPlaylistToDB(playlist_id, output, std::move(value));
}

void PlaylistsController::PutThumbnailReadyPlaylistToDB(
    const std::string& key,
    const std::string& json_value,
    base::Value&& playlist_value) {
  base::PostTaskAndReplyWithResult(
      io_task_runner(),
      FROM_HERE,
      base::BindOnce(&PlaylistsDBController::Put,
                     base::Unretained(db_controller_.get()),
                     key,
                     json_value),
      base::BindOnce(&PlaylistsController::OnPutThumbnailReadyPlaylist,
                     weak_factory_.GetWeakPtr(),
                     std::move(playlist_value)));
}

void PlaylistsController::OnPutThumbnailReadyPlaylist(
    base::Value&& playlist_value,
    bool result) {
  if (!result)
    return;

  base::Value value = std::move(playlist_value);
  NotifyPlaylistChanged(
      { PlaylistsChangeParams::ChangeType::CHANGE_TYPE_THUMBNAIL_READY,
        *value.FindStringKey("id") });

  // Add to pending jobs
  pending_media_file_creation_jobs_.push(std::move(value));

  // If |media_file_controller_| is generating other playlist media file,
  // next playlist generation will be triggered when current one is finished.
  if (!media_file_controller_->in_progress())
     GenerateMediaFile();
}

void PlaylistsController::GenerateMediaFile() {
  DCHECK(!media_file_controller_->in_progress());
  DCHECK(!pending_media_file_creation_jobs_.empty());

  base::Value value(std::move(pending_media_file_creation_jobs_.front()));
  pending_media_file_creation_jobs_.pop();
  LOG(ERROR) << __func__ << ": " << *value.FindStringKey("playlistName");

  media_file_controller_->GenerateSingleMediaFile(std::move(value));
}

// Store PlaylistInfo to db after getting thumbnail and notify it.
// Then notify again when it's ready to play.
// TODO(simonhong): Add basic validation for |params|.
bool PlaylistsController::CreatePlaylist(const CreatePlaylistParams& params) {
  DCHECK(initialized_);
  PlaylistInfo p = CreatePlaylistInfo(params);

  base::Value value = GetValueFromPlaylistInfo(p);
  std::string output;
  base::JSONWriter::Write(value, &output);
  PutInitialPlaylistToDB(p.id, output, std::move(value));

  return true;
}

void PlaylistsController::PutInitialPlaylistToDB(const std::string& key,
                                                 const std::string& json_value,
                                                 base::Value&& playlist_value) {
  base::PostTaskAndReplyWithResult(
      io_task_runner(),
      FROM_HERE,
      base::BindOnce(&PlaylistsDBController::Put,
                     base::Unretained(db_controller_.get()),
                     key,
                     json_value),
      base::BindOnce(&PlaylistsController::OnPutInitialPlaylist,
                     weak_factory_.GetWeakPtr(),
                     std::move(playlist_value)));
}

void PlaylistsController::OnPutInitialPlaylist(base::Value&& playlist_value,
                                               bool result) {
  if (!result)
    return;

  base::Value value = std::move(playlist_value);
  const std::string playlist_id = *value.FindStringKey("id");
  NotifyPlaylistChanged({ PlaylistsChangeParams::ChangeType::CHANGE_TYPE_ADDED,
                          playlist_id });
  const base::FilePath playlist_dir = base_dir_.Append(playlist_id);
  base::PostTaskAndReplyWithResult(
      io_task_runner(),
      FROM_HERE,
      base::BindOnce(&EnsureDirectoryExists, playlist_dir),
      base::BindOnce(&PlaylistsController::DownloadThumbnail,
                     weak_factory_.GetWeakPtr(),
                     std::move(value)));
}

void PlaylistsController::GetAllPlaylists() {
  DCHECK(initialized_);
}

void PlaylistsController::GetPlaylist(const std::string& id) {
  DCHECK(initialized_);

  base::PostTaskAndReplyWithResult(
    io_task_runner(),
    FROM_HERE,
    base::BindOnce(&PlaylistsDBController::Get,
                   base::Unretained(db_controller_.get()),
                   id),
    base::BindOnce(&PlaylistsController::OnGetPlaylist,
                   weak_factory_.GetWeakPtr()));
}

void PlaylistsController::OnGetPlaylist(const std::string& value) {
  if (value.empty())
    return;

  base::Optional<base::Value> playlist = base::JSONReader::Read(value);
  if (playlist) {
    base::Value ret(base::Value::Type::DICTIONARY);
    ret.SetStringKey("id", std::move(*playlist->FindStringKey("id")));
    ret.SetStringKey("playlistName",
                     std::move(*playlist->FindStringKey("playlistName")));
    ret.SetStringKey("thumbnailPath",
                     std::move(*playlist->FindStringKey("thumbnailPath")));
    ret.SetStringKey("mediaFilePath",
                     std::move(*playlist->FindStringKey("mediaFilePath")));
    ret.SetKey("titles", std::move(*playlist->FindListKey("titles")));
    PrintValue(ret);
  }
}

void PlaylistsController::DeletePlaylist(const std::string& id) {
  DCHECK(initialized_);
}

void PlaylistsController::DeleteAllPlaylists() {
  DCHECK(initialized_);
  NOTIMPLEMENTED();
}

void PlaylistsController::AddObserver(PlaylistsControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void PlaylistsController::RemoveObserver(
    PlaylistsControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PlaylistsController::OnMediaFileReady(base::Value&& playlist_value) {
  base::Value value(std::move(playlist_value));
  LOG(ERROR) << __func__ << ": " << *value.FindStringKey("playlistName");

  std::string output;
  base::JSONWriter::Write(value, &output);
  const std::string playlist_id = *value.FindStringKey("id");

  PutPlayReadyPlaylistToDB(playlist_id, output, std::move(value));

  if (!pending_media_file_creation_jobs_.empty())
    GenerateMediaFile();
}

void PlaylistsController::OnMediaFileGenerationFailed(
    base::Value&& playlist_value) {
  base::Value value(std::move(playlist_value));
  LOG(ERROR) << __func__ << ": " << *value.FindStringKey("playlistName");

  std::string output;
  base::JSONWriter::Write(value, &output);
  const std::string playlist_id = *value.FindStringKey("id");

  if (!pending_media_file_creation_jobs_.empty())
    GenerateMediaFile();
}

void PlaylistsController::PutAbortedPlaylistToDB(const std::string& key,
                                                 const std::string& json_value,
                                                 base::Value&& playlist_value) {
  base::PostTaskAndReplyWithResult(
      io_task_runner(),
      FROM_HERE,
      base::BindOnce(&PlaylistsDBController::Put,
                     base::Unretained(db_controller_.get()),
                     key,
                     json_value),
      base::BindOnce(&PlaylistsController::OnPutPlayReadyPlaylist,
                     weak_factory_.GetWeakPtr(),
                     std::move(playlist_value)));
}

void PlaylistsController::OnPutAbortedPlaylist(base::Value&& playlist_value,
                                               bool result) {
  if (!result)
    return;

  base::Value value = std::move(playlist_value);
  const std::string playlist_id = *value.FindStringKey("id");
  NotifyPlaylistChanged(
      { PlaylistsChangeParams::ChangeType::CHANGE_TYPE_ABORTED,
        playlist_id });
}

void PlaylistsController::PutPlayReadyPlaylistToDB(
    const std::string& key,
    const std::string& json_value,
    base::Value&& playlist_value) {
  base::PostTaskAndReplyWithResult(
      io_task_runner(),
      FROM_HERE,
      base::BindOnce(&PlaylistsDBController::Put,
                     base::Unretained(db_controller_.get()),
                     key,
                     json_value),
      base::BindOnce(&PlaylistsController::OnPutPlayReadyPlaylist,
                     weak_factory_.GetWeakPtr(),
                     std::move(playlist_value)));
}

void PlaylistsController::OnPutPlayReadyPlaylist(base::Value&& playlist_value,
                                                 bool result) {
  if (!result)
    return;

  base::Value value = std::move(playlist_value);
  const std::string playlist_id = *value.FindStringKey("id");
  NotifyPlaylistChanged(
      { PlaylistsChangeParams::ChangeType::CHANGE_TYPE_PLAY_READY,
        playlist_id });
}

base::SequencedTaskRunner* PlaylistsController::io_task_runner() {
  if (!io_task_runner_) {
    io_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        { base::MayBlock(),
          base::TaskPriority::BEST_EFFORT,
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN });
  }
  return io_task_runner_.get();
}
