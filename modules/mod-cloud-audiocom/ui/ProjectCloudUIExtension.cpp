/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ProjectCloudUIExtension.cpp

  Dmitry Vedenko

**********************************************************************/
#include "ProjectCloudUIExtension.h"

#include <wx/log.h>

#include "sync/ProjectCloudExtension.h"

#include "dialogs/ConnectionIssuesDialog.h"
#include "dialogs/NotCloudProjectDialog.h"
#include "dialogs/ProjectLimitDialog.h"
#include "dialogs/ProjectVersionConflictDialog.h"
#include "dialogs/SyncSuccessDialog.h"
#include "dialogs/WaitForActionDialog.h"

#include "CloudProjectUtils.h"
#include "OAuthService.h"

#include "BasicUI.h"
#include "CodeConversions.h"
#include "Project.h"

namespace cloud::audiocom::sync
{
namespace
{
const AttachedProjectObjects::RegisteredFactory key {
   [](AudacityProject& project)
   { return std::make_shared<ProjectCloudUIExtension>(project); }
};
} // namespace

ProjectCloudUIExtension::ProjectCloudUIExtension(AudacityProject& project)
    : mProject { project }
    , mCloudStatusChangedSubscription {
       ProjectCloudExtension::Get(project).SubscribeStatusChanged(
          [this](const auto& status) { OnCloudStatusChanged(status); }, true)
    }
{
}

ProjectCloudUIExtension::~ProjectCloudUIExtension() = default;

ProjectCloudUIExtension& ProjectCloudUIExtension::Get(AudacityProject& project)
{
   return project.AttachedObjects::Get<ProjectCloudUIExtension&>(key);
}

const ProjectCloudUIExtension&
ProjectCloudUIExtension::Get(const AudacityProject& project)
{
   return Get(const_cast<AudacityProject&>(project));
}

bool ProjectCloudUIExtension::SetUploadProgress(double progress)
{
   mProgress = progress;

   if (mProgressDialog == nullptr)
      return true;

   const auto result = mProgressDialog->Poll(progress * 10000, 10000);

   if (result == BasicUI::ProgressResult::Cancelled)
   {
      mClosingCancelled = true;
      mProgressDialog.reset();
      return true;
   }

   return result != BasicUI::ProgressResult::Stopped;
}

bool ProjectCloudUIExtension::AllowClosing() const
{
   while (mInSync.load(std::memory_order_acquire) && !mClosingCancelled)
   {
      if (mProgressDialog == nullptr)
      {
         mProgressDialog = BasicUI::MakeProgress(
            XO("Save to audio.com"),
            XO("Project is syncing with audio.com. Do you want to stop the sync process?"),
            BasicUI::ProgressShowCancel | BasicUI::ProgressShowStop);
      }

      BasicUI::Yield();
   }

   return !mInSync.load(std::memory_order_acquire) || !mClosingCancelled;
}

void ProjectCloudUIExtension::OnCloudStatusChanged(
   const CloudStatusChangedMessage& message)
{
   mInSync = message.IsSyncing();

   if (!mNeedsFirstSaveDialog)
   {
      const auto savesCount =
         ProjectCloudExtension::Get(mProject).GetSavesCount();
      mNeedsFirstSaveDialog = savesCount == 0;
   }

   if (!mInSync)
   {
      mProgressDialog.reset();
      if (mNeedsFirstSaveDialog)
      {
         mNeedsFirstSaveDialog = false;

         if (
            SyncSuccessDialog { &mProject }.ShowDialog() ==
            SyncSuccessDialog::ViewOnlineIdentifier())
         {
            // TODO: Open the project in the browser
         }
      }
   }
   else
   {
      SetUploadProgress(message.Progress);
   }

   if (message.Status != ProjectSyncStatus::Failed || !message.Error)
      return;

   const auto error = *message.Error;

   switch (error.Type)
   {
   case CloudSyncError::Authorization:
      // How do we got here? Probable auth_token is invalid?
      GetOAuthService().UnlinkAccount();
      SaveToCloud(mProject, SaveMode::Normal);
      break;
   case CloudSyncError::ProjectLimitReached:
      [[fallthrough]];
   case CloudSyncError::ProjectStorageLimitReached:
   {
      auto result = ProjectLimitDialog { &mProject }.ShowDialog();

      if (result == ProjectLimitDialog::VisitAudioComIdentifier())
      {
         WaitForActionDialog { &mProject,
                               XO("Please, complete your action on audio.com"),
                               true }
            .ShowDialog();
         SaveToCloud(mProject, SaveMode::Normal);
      }
      else
      {
         if (!ResaveLocally(mProject))
            SaveToCloud(mProject, SaveMode::Normal);
      }
   }
   break;
   case CloudSyncError::ProjectVersionConflict:
   {
      if (
         ProjectVersionConflictDialog { &mProject, true }.ShowDialog() ==
         ProjectVersionConflictDialog::UseLocalIdentifier())
      {
         SaveToCloud(mProject, SaveMode::ForceSave);
      }
      else
      {
         ReopenProject(mProject);
      }
   }
   break;
   case CloudSyncError::ProjectNotFound:
   {
      if (
         NotCloudProjectDialog { &mProject }.ShowDialog() ==
         NotCloudProjectDialog::SaveLocallyIdentifier())
      {
         if (!ResaveLocally(mProject))
            SaveToCloud(mProject, SaveMode::SaveNew);
      }
      else
      {
         SaveToCloud(mProject, SaveMode::SaveNew);
      }
   }
   break;
   case CloudSyncError::Network:
   {
      ConnectionIssuesDialog { &mProject }.ShowDialog();
   }
   break;
   case CloudSyncError::DataUploadFailed:
      [[fallthrough]];
   case CloudSyncError::Server:
      [[fallthrough]];
   case CloudSyncError::ClientFailure:
      BasicUI::ShowErrorDialog(
         *ProjectFramePlacement(&mProject), XO("Save to cloud"),
         XO("Failed to save the project to the cloud"), {},
         BasicUI::ErrorDialogOptions {}.Log(
            audacity::ToWString(error.ErrorMessage)));
      break;
   case CloudSyncError::Cancelled:
      [[fallthrough]];
   default:
      break;
   }

   wxLogError(
      "Cloud sync has failed: %s", audacity::ToWXString(error.ErrorMessage));
}

} // namespace cloud::audiocom::sync