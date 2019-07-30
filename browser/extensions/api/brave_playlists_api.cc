/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/extensions/api/brave_playlists_api.h"

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "brave/common/extensions/api/brave_playlists.h"
#include "brave/components/playlists/browser/playlists_controller.h"
#include "brave/components/playlists/browser/playlists_service.h"
#include "brave/components/playlists/browser/playlists_service_factory.h"
#include "brave/components/playlists/browser/playlists_types.h"
#include "chrome/browser/profiles/profile.h"

namespace CreatePlaylist = extensions::api::brave_playlists::CreatePlaylist;
namespace GetPlaylist = extensions::api::brave_playlists::GetPlaylist;
namespace DeletePlaylist = extensions::api::brave_playlists::DeletePlaylist;

namespace {
constexpr char kNotInitializedError[] = "Not initialized";
constexpr char kAlreadyInitializedError[] = "Already initialized";
constexpr char kInvalidArgsError[] = "Invalid arguments";

PlaylistsController* GetPlaylistsController(content::BrowserContext* context) {
  return PlaylistsServiceFactory::GetInstance()->GetForProfile(
      Profile::FromBrowserContext(context))->controller();
}

CreatePlaylistParams GetCreatePlaylistParamsFromCreateParams(
    const CreatePlaylist::Params::CreateParams& params) {
  CreatePlaylistParams p;
  p.playlist_name = params.playlist_name;
  p.playlist_thumbnail_url = params.thumbnail_url;

  for (const auto& file : params.media_files)
    p.media_files.emplace_back(file.url, file.title);
  return p;
}

}  // namespace

namespace extensions {
namespace api {

ExtensionFunction::ResponseAction BravePlaylistsCreatePlaylistFunction::Run() {
  if (!GetPlaylistsController(browser_context())->initialized())
    return RespondNow(Error(kNotInitializedError));

  std::unique_ptr<CreatePlaylist::Params> params(
      CreatePlaylist::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (GetPlaylistsController(browser_context())->CreatePlaylist(
          GetCreatePlaylistParamsFromCreateParams(params->create_params))) {
    return RespondNow(NoArguments());
  }

  return RespondNow(Error(kInvalidArgsError));
}

ExtensionFunction::ResponseAction BravePlaylistsIsInitializedFunction::Run() {
  const bool initialized =
      GetPlaylistsController(browser_context())->initialized();
  return RespondNow(OneArgument(std::make_unique<base::Value>(initialized)));
}

ExtensionFunction::ResponseAction BravePlaylistsInitializeFunction::Run() {
  if (GetPlaylistsController(browser_context())->initialized())
    return RespondNow(Error(kAlreadyInitializedError));

  PlaylistsServiceFactory::GetInstance()->GetForProfile(
        Profile::FromBrowserContext(browser_context()))->Init();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction BravePlaylistsGetAllPlaylistsFunction::Run() {
  if (!GetPlaylistsController(browser_context())->initialized())
    return RespondNow(Error(kNotInitializedError));

  GetPlaylistsController(browser_context())->GetAllPlaylists();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction BravePlaylistsGetPlaylistFunction::Run() {
  if (!GetPlaylistsController(browser_context())->initialized())
    return RespondNow(Error(kNotInitializedError));

  std::unique_ptr<GetPlaylist::Params> params(
      GetPlaylist::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GetPlaylistsController(browser_context())->GetPlaylist(params->id);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction BravePlaylistsDeletePlaylistFunction::Run() {
  if (!GetPlaylistsController(browser_context())->initialized())
    return RespondNow(Error(kNotInitializedError));

  std::unique_ptr<DeletePlaylist::Params> params(
      DeletePlaylist::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GetPlaylistsController(browser_context())->DeletePlaylist(params->id);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
BravePlaylistsDeleteAllPlaylistsFunction::Run() {
  if (!GetPlaylistsController(browser_context())->initialized())
    return RespondNow(Error(kNotInitializedError));

  GetPlaylistsController(browser_context())->DeleteAllPlaylists();

  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace extensions
