﻿#include <set>
#include <fstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/archives/portable_binary.hpp>

#include <QFileInfo>
#include <QCoreApplication>
#include <QMessageBox>
#include <QSettings>

#include "hs_cartographics/cartographics_format/formatter_proj4.hpp"
#include "hs_cartographics/cartographics_conversion/convertor.hpp"
#include "hs_sfm/sfm_utility/camera_type.hpp"

//#include "workflow/feature_match//openmvg_feature_match.hpp"
#include "workflow/feature_match/opencv_feature_match.hpp"
#include "workflow/mesh_surface/delaunay_surface_model.hpp"

#include "gui/blocks_pane.hpp"
#include "gui/block_photos_select_dialog.hpp"
#include "gui/main_window.hpp"
#include "gui/workflow_configure_dialog.hpp"
#include "gui/default_longitude_latitude_convertor.hpp"

namespace hs
{
namespace recon
{
namespace gui
{

BlocksPane::BlocksPane(QWidget* parent)
  : ManagerPane(tr("Blocks"), parent)
  , icon_add_block_(":/images/icon_block_add.png")
  , icon_copy_(":/images/icon_copy.png")
  , icon_remove_(":/images/icon_remove.png")
  , icon_add_workflow_(":/images/icon_workflow_add.png")
  , icon_info_(":/images/icon_info.png")
  , selected_block_id_(std::numeric_limits<uint>::max())
  , selected_feature_match_id_(std::numeric_limits<uint>::max())
  , selected_photo_orientation_id_(std::numeric_limits<uint>::max())
  , selected_point_cloud_id_(std::numeric_limits<uint>::max())
  , selected_surface_model_id_(std::numeric_limits<uint>::max())
  , selected_texture_id_(std::numeric_limits<uint>::max())
  , activated_photo_orientation_id_(std::numeric_limits<uint>::max())
  , activated_point_cloud_id_(std::numeric_limits<uint>::max())
  , activated_surface_model_id_(std::numeric_limits<uint>::max())
{
  timer_ = new QTimer(this);
  blocks_tree_widget_ = new BlocksTreeWidget(this);
  photo_orientation_info_widget_ = new PhotoOrientationInfoWidget(this);

  QTreeWidgetItem* header_item =
    new QTreeWidgetItem(QStringList()<<tr("Name")<<tr("Progress"));
  blocks_tree_widget_->setHeaderItem(header_item);
  AddWidget(blocks_tree_widget_);
  AddWidget(photo_orientation_info_widget_);
  photo_orientation_info_widget_->hide();


  progress_bar_ = new QProgressBar(this);
  progress_bar_->hide();

  action_add_block_ = new QAction(icon_add_block_,
                                  tr("Add Block"), this);
  action_copy_ = new QAction(icon_copy_,
                             tr("Remove Block"), this);
  action_copy_->setEnabled(false);
  action_remove_ = new QAction(icon_remove_,
                               tr("Remove Photos"), this);
  action_remove_->setEnabled(false);
  action_add_workflow_ = new QAction(icon_add_workflow_,
                                     tr("Add Workflow"), this);
  action_add_workflow_->setEnabled(false);

  action_info_ = new QAction(icon_info_,tr("Information"), this);
  action_info_->setEnabled(false);

  tool_bar_ = new QToolBar(this);
  tool_bar_->addAction(action_add_block_);
  tool_bar_->addAction(action_copy_);
  tool_bar_->addAction(action_remove_);
  tool_bar_->addAction(action_add_workflow_);
  tool_bar_->addAction(action_info_);

  main_window_->addToolBar(tool_bar_);

  QObject::connect(action_add_block_, &QAction::triggered,
                   this, &BlocksPane::OnActionAddBlockTriggered);
  QObject::connect(action_add_workflow_, &QAction::triggered,
                   this, &BlocksPane::OnActionAddWorkflowTriggered);
  QObject::connect(action_copy_, &QAction::triggered,
                   this, &BlocksPane::OnActionCopyTriggered);
  QObject::connect(action_remove_, &QAction::triggered,
                   this, &BlocksPane::OnActionRemoveTriggered);
  QObject::connect(action_info_, &QAction::triggered,
                   this, &BlocksPane::OnActionInfoTriggered);

  QObject::connect(timer_, &QTimer::timeout, this, &BlocksPane::OnTimeout);
  QObject::connect(blocks_tree_widget_, &QTreeWidget::itemDoubleClicked,
                   this, &BlocksPane::OnItemDoubleClicked);

  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::BlockItemSelected,
    this, &BlocksPane::OnBlockItemSelected);
  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::PhotosInOneBlockSelected,
    this, &BlocksPane::OnPhotosInOneBlockSelected);
  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::PhotosItemSelected,
    this, &BlocksPane::OnPhotosItemSelected);
  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::FeatureMatchItemSelected,
    this, &BlocksPane::OnFeatureMatchItemSelected);
  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::PhotoOrientationItemSelected,
    this, &BlocksPane::OnPhotoOrientationItemSelected);
  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::PointCloudItemSelected,
    this, &BlocksPane::OnPointCloudItemSelected);
  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::SurfaceModelItemSelected,
    this, &BlocksPane::OnSurfaceModelItemSelected);
  QObject::connect(
    blocks_tree_widget_, &BlocksTreeWidget::TextureItemSelected,
    this, &BlocksPane::OnTextureItemSelected);

}

void BlocksPane::Response(int request_flag, void* response)
{
  switch (request_flag)
  {
  case db::DatabaseMediator::REQUEST_OPEN_DATABASE:
    {
      db::ResponseOpenDatabase* response_open =
        static_cast<db::ResponseOpenDatabase*>(response);
      if (response_open->error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
      {
        while (1)
        {
          db::RequestGetAllBlocks blocks_request;
          db::ResponseGetAllBlocks blocks_response;
          ((MainWindow*)parent())->database_mediator().Request(
            this, db::DatabaseMediator::REQUEST_GET_ALL_BLOCKS,
            blocks_request, blocks_response, false);
          if (blocks_response.error_code !=
              db::DatabaseMediator::DATABASE_NO_ERROR)
            break;

          auto itr_block = blocks_response.records.begin();
          auto itr_block_end = blocks_response.records.end();
          for (; itr_block != itr_block_end; ++itr_block)
          {
            uint block_id = uint(itr_block->first);
            std::string block_std_name =
              itr_block->second[db::BlockResource::BLOCK_FIELD_NAME].ToString();
            QString block_name = QString::fromLocal8Bit(block_std_name.c_str());
            blocks_tree_widget_->AddBlock(block_id, block_name);

            hs::recon::db::RequestGetPhotosInBlock request_get_photos_in_block;
            hs::recon::db::ResponseGetPhotosInBlock response_get_photos_in_block;
            request_get_photos_in_block.block_id =
              db::Database::Identifier(block_id);
            ((MainWindow*)parent())->database_mediator().Request(
              this, db::DatabaseMediator::REQUEST_GET_PHOTOS_IN_BLOCK,
              request_get_photos_in_block, response_get_photos_in_block, false);

            if (response_get_photos_in_block.error_code !=
                hs::recon::db::Database::DATABASE_NO_ERROR)
            {
              continue;
            }
            auto itr_photo = response_get_photos_in_block.records.begin();
            auto itr_photo_end = response_get_photos_in_block.records.end();
            BlocksTreeWidget::StringMap photo_names;
            for (; itr_photo != itr_photo_end; ++itr_photo)
            {
              std::string photo_path =
                itr_photo->second[db::PhotoResource::PHOTO_FIELD_PATH].ToString();
              QFileInfo file_info(QString::fromLocal8Bit(photo_path.c_str()));
              photo_names[uint(itr_photo->first)] = file_info.fileName();
            }
            blocks_tree_widget_->AddPhotosToBlock(block_id, photo_names);
          }

          hs::recon::db::RequestGetAllFeatureMatches feature_matches_request;
          hs::recon::db::ResponseGetAllFeatureMatches feature_matches_response;
          ((MainWindow*)parent())->database_mediator().Request(
            this, db::DatabaseMediator::REQUEST_GET_ALL_FEATURE_MATCHES,
            feature_matches_request, feature_matches_response, false);
          if (feature_matches_response.error_code !=
              db::DatabaseMediator::DATABASE_NO_ERROR)
            break;

          auto itr_feature_match = feature_matches_response.records.begin();
          auto itr_feature_match_end = feature_matches_response.records.end();
          for (; itr_feature_match != itr_feature_match_end; ++itr_feature_match)
          {
            uint feature_match_id = itr_feature_match->first;
            uint block_id =
              uint(itr_feature_match->second[
                db::FeatureMatchResource::FEATURE_MATCH_FIELD_BLOCK_ID].ToInt());
            std::string feature_match_std_name =
              itr_feature_match->second[
                db::FeatureMatchResource::FEATURE_MATCH_FIELD_NAME].ToString();
            QString feature_match_name =
              QString::fromLocal8Bit(feature_match_std_name.c_str());
            int flag =
              itr_feature_match->second[
                db::FeatureMatchResource::FEATURE_MATCH_FIELD_FLAG].ToInt();
            if (blocks_tree_widget_->AddFeatureMatch(
                  block_id, feature_match_id, feature_match_name) == 0 &&
                flag == db::FeatureMatchResource::FLAG_NOT_COMPLETED)
            {
              QTreeWidgetItem* item =
                blocks_tree_widget_->FeatureMatchItem(feature_match_id);
              item->setTextColor(0, Qt::gray);
            }
          }

          db::RequestGetAllPhotoOrientations photo_orientations_request;
          db::ResponseGetAllPhotoOrientations photo_orientations_response;
          ((MainWindow*)parent())->database_mediator().Request(
            this, db::DatabaseMediator::REQUEST_GET_ALL_PHOTO_ORIENTATIONS,
            photo_orientations_request, photo_orientations_response, false);
          if (photo_orientations_response.error_code !=
              db::DatabaseMediator::DATABASE_NO_ERROR)
            break;

          auto itr_photo_orientation =
            photo_orientations_response.records.begin();
          auto itr_photo_orientation_end =
            photo_orientations_response.records.end();
          for (; itr_photo_orientation != itr_photo_orientation_end;
               ++itr_photo_orientation)
          {
            uint photo_orientation_id = itr_photo_orientation->first;
            uint feature_match_id =
              uint(itr_photo_orientation->second[
                db::PhotoOrientationResource::
                  PHOTO_ORIENTATION_FIELD_FEATURE_MATCH_ID].ToInt());
            std::string photo_orientation_std_name =
              itr_photo_orientation->second[
                db::PhotoOrientationResource::
                  PHOTO_ORIENTATION_FIELD_NAME].ToString();
            QString photo_orientation_name =
              QString::fromLocal8Bit(photo_orientation_std_name.c_str());
            int flag =
              itr_photo_orientation->second[
                db::PhotoOrientationResource::
                  PHOTO_ORIENTATION_FIELD_FLAG].ToInt();
            if (blocks_tree_widget_->AddPhotoOrientation(
                  feature_match_id, photo_orientation_id,
                  photo_orientation_name) == 0 &&
                flag == db::PhotoOrientationResource::FLAG_NOT_COMPLETED)
            {
              QTreeWidgetItem* item =
                blocks_tree_widget_->PhotoOrientationItem(photo_orientation_id);
              item->setTextColor(0, Qt::gray);
            }
          }

          //读取点云数据
          db::RequestGetAllPointClouds point_cloud_request;
          db::ResponseGetAllPointClouds point_cloud_response;
          ((MainWindow*)parent())->database_mediator().Request(
            this, db::DatabaseMediator::REQUEST_GET_ALL_POINT_CLOUDS,
            point_cloud_request, point_cloud_response, false);
          if (point_cloud_response.error_code !=
            db::DatabaseMediator::DATABASE_NO_ERROR)
            break;

          auto itr_point_cloud =
            point_cloud_response.records.begin();
          auto itr_point_cloud_end =
            point_cloud_response.records.end();
          for (; itr_point_cloud != itr_point_cloud_end;
            ++itr_point_cloud)
          {
            uint point_cloud_id = itr_point_cloud->first;
            uint photo_orientation_id =
              uint(itr_point_cloud->second[
              db::PointCloudResource::
              POINT_CLOUD_FIELD_PHOTO_ORIENTATION_ID].ToInt());
            std::string point_cloud_std_name =
              itr_point_cloud->second[
              db::PointCloudResource::
              POINT_CLOUD_FIELD_NAME].ToString();
            QString point_cloud_name =
            QString::fromLocal8Bit(point_cloud_std_name.c_str());
            int flag =
            itr_point_cloud->second[
              db::PointCloudResource::
              POINT_CLOUD_FIELD_FLAG].ToInt();
            if (blocks_tree_widget_->AddPointCloud(
              photo_orientation_id, point_cloud_id,
              point_cloud_name) == 0 &&
              flag == db::PointCloudResource::FLAG_NOT_COMPLETED)
            {
              QTreeWidgetItem* item =
                blocks_tree_widget_->PointCloudItem(point_cloud_id);
              item->setTextColor(0, Qt::gray);
            }
          }

          //读取model
          db::RequestGetAllSurfaceModels surface_model_request;
          db::ResponseGetAllSurfaceModels surface_model_response;
          ((MainWindow*)parent())->database_mediator().Request(
            this, db::DatabaseMediator::REQUEST_GET_ALL_SURFACE_MODELS,
            surface_model_request, surface_model_response, false);
          if (surface_model_response.error_code !=
            db::DatabaseMediator::DATABASE_NO_ERROR)
            break;

          auto itr_surface_model =
            surface_model_response.records.begin();
          auto itr_surface_model_end =
            surface_model_response.records.end();
          for (; itr_surface_model != itr_surface_model_end;
            ++itr_surface_model)
          {
            uint surface_model_id = itr_surface_model->first;
            uint point_cloud_id =
              uint(itr_surface_model->second[
                db::SurfaceModelResource::
                  SURFACE_MODEL_FIELD_POINT_CLOUD_ID].ToInt());
            std::string surface_model_std_name =
              itr_surface_model->second[
                db::SurfaceModelResource::
                  SURFACE_MODEL_FIELD_NAME].ToString();
            QString surface_model_name =
              QString::fromLocal8Bit(surface_model_std_name.c_str());
            int flag =
              itr_surface_model->second[
                db::SurfaceModelResource::
                  SURFACE_MODEL_FIELD_FLAG].ToInt();
            if (blocks_tree_widget_->AddSurfaceModel(
              point_cloud_id, surface_model_id,
              surface_model_name) == 0 &&
              flag == db::SurfaceModelResource::FLAG_NOT_COMPLETED)
            {
              QTreeWidgetItem* item =
                blocks_tree_widget_->SurfaceModelItem(surface_model_id);
              item->setTextColor(0, Qt::gray);
            }
          }

          //读取Texture
          db::RequestGetAllTextures texture_request;
          db::ResponseGetAllTextures texture_response;
          ((MainWindow*)parent())->database_mediator().Request(
            this, db::DatabaseMediator::REQUEST_GET_ALL_TEXTURES,
            texture_request, texture_response, false);
          if (texture_response.error_code != db::DatabaseMediator::DATABASE_NO_ERROR)
            break;

          auto itr_texture = texture_response.records.begin();
          auto itr_texture_end = texture_response.records.end();
          for (; itr_texture != itr_texture_end; ++itr_texture)
          {
            uint texture_id = uint(itr_texture->first);
            uint surface_model_id =
              uint(itr_texture->second[
                db::TextureResource::TEXTURE_FIELD_SURFACE_MODEL_ID].ToInt());
            std::string texture_std_name =
              itr_texture->second[
                db::TextureResource::TEXTURE_FIELD_NAME].ToString();
            QString texture_name =
              QString::fromLocal8Bit(texture_std_name.c_str());
            int flag = itr_texture->second[
                         db::TextureResource::TEXTURE_FIELD_FLAG].ToInt();
            if (blocks_tree_widget_->AddTexture(surface_model_id, texture_id,
                                                texture_name) == 0 &&
                flag == db::TextureResource::FLAG_NOT_COMPLETED)
            {
              QTreeWidgetItem* item =
                blocks_tree_widget_->TextureItem(texture_id);
              item->setTextColor(0, Qt::gray);
            }
          }
          break;
        }
      }

      break;
    }
  case db::DatabaseMediator::REQUEST_COPY_PHOTO_ORIENTATION:
    {
      db::ResponseCopyPhotoOrientation* response_copy_photo_orientation =
        static_cast<db::ResponseCopyPhotoOrientation*>(response);
      QString copied_photo_orientation_name =
        QString::fromLocal8Bit(
          response_copy_photo_orientation->
            copied_photo_orientation_name.c_str());
      uint feature_match_id =
        uint(response_copy_photo_orientation->feature_match_id);
      uint photo_orientation_id =
        uint(response_copy_photo_orientation->copied_photo_orientation_id);
      blocks_tree_widget_->AddPhotoOrientation(
        feature_match_id, photo_orientation_id, copied_photo_orientation_name);

      QTreeWidgetItem* item =
        blocks_tree_widget_->PhotoOrientationItem(photo_orientation_id);
      ActivatePhotoOrientationItem(item);
      emit PhotoOrientationActivated(activated_photo_orientation_id_);
      break;
    }
  case db::DatabaseMediator::REQUEST_CLOSE_DATABASE:
    {
      db::ResponseCloseDatabase* response_close =
        static_cast<db::ResponseCloseDatabase*>(response);
      if (response_close->error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
      {
        blocks_tree_widget_->Clear();
        action_copy_->setEnabled(false);
        action_remove_->setEnabled(false);
        action_add_workflow_->setEnabled(false);
        photo_orientation_info_widget_->hide();

        item_selected_mask_.reset();
      }
      break;
    }
  }
}

void BlocksPane::OnTimeout()
{
  if (workflow_queue_.empty())
  {
    progress_bar_->hide();
    timer_->stop();
  }
  else
  {
    if (current_step_)
    {
      int state = current_step_->state();
      switch (state)
      {
      case hs::recon::workflow::WorkflowStep::STATE_READY:
        {
          WorkflowConfig& workflow = workflow_queue_.front();
          WorkflowStepEntry& workflow_step_entry = workflow.step_queue.front();
          current_step_->Start(workflow_step_entry.config.get());
          QTreeWidgetItem* item = nullptr;
          switch (workflow_step_entry.config->type())
          {
          case workflow::STEP_FEATURE_MATCH:
            {
              item = blocks_tree_widget_->FeatureMatchItem(
                       workflow_step_entry.id);
              break;
            }
          case workflow::STEP_PHOTO_ORIENTATION:
            {
              item = blocks_tree_widget_->PhotoOrientationItem(
                       workflow_step_entry.id);
              break;
            }
          case workflow::STEP_POINT_CLOUD:
            {
              item = blocks_tree_widget_->PointCloudItem(
                       workflow_step_entry.id);
              break;
            }
          case workflow::STEP_SURFACE_MODEL:
            {
              item = blocks_tree_widget_->SurfaceModelItem(
                       workflow_step_entry.id);
              break;
            }
          case workflow::STEP_TEXTURE:
            {
              item = blocks_tree_widget_->TextureItem(
                       workflow_step_entry.id);
              break;
            }
          }
          if (item)
          {
            blocks_tree_widget_->setItemWidget(item, 1, progress_bar_);
            progress_bar_->show();
            progress_bar_->setMinimum(0);
            progress_bar_->setMaximum(100);
            progress_bar_->setValue(0);
          }
          break;
        }
      case hs::recon::workflow::WorkflowStep::STATE_WORKING:
        {
          float complete_ratio = current_step_->GetCompleteRatio();
          progress_bar_->setValue(int(complete_ratio * 100));
          break;
        }
      case hs::recon::workflow::WorkflowStep::STATE_ERROR:
        {
          //终止该Workflow
          workflow_queue_.pop();
          current_step_ = nullptr;
          progress_bar_->hide();
          break;
        }
      case hs::recon::workflow::WorkflowStep::STATE_FINISHED:
        {
          progress_bar_->setValue(100);
          WorkflowConfig& workflow_config = workflow_queue_.front();
          WorkflowStepEntry workflow_step_entry =
            workflow_config.step_queue.front();

          switch (workflow_step_entry.config->type())
          {
          case workflow::STEP_FEATURE_MATCH:
            {
              db::RequestUpdateFeatureMatchFlag request;
              db::ResponseUpdateFeatureMatchFlag response;
              request.id = db::Database::Identifier(workflow_step_entry.id);
              request.flag = db::FeatureMatchResource::FLAG_COMPLETED;
              ((MainWindow*)parent())->database_mediator().Request(
                this, db::DatabaseMediator::REQUEST_UPDATE_FEATURE_MATCH_FLAG,
                request, response, true);
              if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
              {
                QTreeWidgetItem* item =
                  blocks_tree_widget_->FeatureMatchItem(
                    workflow_step_entry.id);
                if (item)
                {
                  item->setTextColor(0, Qt::black);
                }
              }
              break;
            }
          case workflow::STEP_PHOTO_ORIENTATION:
            {
              db::RequestUpdatePhotoOrientationFlag request;
              db::ResponseUpdatePhotoOrientationFlag response;
              request.id = db::Database::Identifier(workflow_step_entry.id);
              request.flag = db::PhotoOrientationResource::FLAG_COMPLETED;
              if (current_step_->result_code() &
                workflow::IncrementalPhotoOrientation::RESULT_GEOREFERENCE)
              {
                request.flag |= db::PhotoOrientationResource::FLAG_GEOREFERENCE;
              }
              ((MainWindow*)parent())->database_mediator().Request(
                this,
                db::DatabaseMediator::REQUEST_UPDATE_PHOTO_ORIENTATION_FLAG,
                request, response, true);
              if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
              {
                QTreeWidgetItem* item =
                  blocks_tree_widget_->PhotoOrientationItem(
                    workflow_step_entry.id);
                if (item)
                {
                  item->setTextColor(0, Qt::black);
                  ActivatePhotoOrientationItem(item);
                  emit PhotoOrientationActivated(
                    activated_photo_orientation_id_);
                }
              }
              break;
            }
          case workflow::STEP_POINT_CLOUD:
            {
              db::RequestUpdatePointCloudFlag request;
              db::ResponseUpdatePointCloudFlag response;
              request.id = db::Database::Identifier(workflow_step_entry.id);
              request.flag = db::PointCloudResource::FLAG_COMPLETED;
              ((MainWindow*)parent())->database_mediator().Request(
                this,
                db::DatabaseMediator::REQUEST_UPDATE_POINT_CLOUD_FLAG,
                request, response, true);
              if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
              {
                QTreeWidgetItem* item =
                  blocks_tree_widget_->PointCloudItem(
                  workflow_step_entry.id);
                if (item)
                {
                  item->setTextColor(0, Qt::black);
                  //ActivatePointCloudItem(item);
                  //emit PointCloudActivated(
                  //  activated_point_cloud_id_);
                }
              }
              break;
            }
          case workflow::STEP_SURFACE_MODEL:
            {
              db::RequestUpdateSurfaceModelFlag request;
              db::ResponseUpdateSurfaceModelFlag response;
              request.id = db::Database::Identifier(workflow_step_entry.id);
              request.flag = db::SurfaceModelResource::FLAG_COMPLETED;
              ((MainWindow*)parent())->database_mediator().Request(
                this,
                db::DatabaseMediator::REQUEST_UPDATE_SURFACE_MODEL_FLAG,
                request, response, true);
              if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
              {
                QTreeWidgetItem* item =
                  blocks_tree_widget_->SurfaceModelItem(
                  workflow_step_entry.id);
                if (item)
                {
                  item->setTextColor(0, Qt::black);
                }
              }
              break;
            }
          case workflow::STEP_TEXTURE:
            {
              db::RequestUpdateTextureFlag request;
              db::ResponseUpdateTextureFlag response;
              request.id = db::Database::Identifier(workflow_step_entry.id);
              request.flag = db::TextureResource::FLAG_COMPLETED;
              ((MainWindow*)parent())->database_mediator().Request(
                this, db::DatabaseMediator::REQUEST_UPDATE_TEXTURE_FLAG,
                request, response, true);
              if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
              {
                QTreeWidgetItem* item =
                  blocks_tree_widget_->TextureItem(workflow_step_entry.id);
                if (item)
                {
                  item->setTextColor(0, Qt::black);
                }
              }
              break;
            }
          }

          workflow_config.step_queue.pop();

          current_step_ = nullptr;
          if (workflow_config.step_queue.empty())
          {
            boost::filesystem::path intermediate_path =
              workflow_config.workflow_intermediate_directory;
            db::RemoveDirectory(intermediate_path);
            workflow_queue_.pop();
          }
          break;
        }
      }
    }
    else
    {
      //开始第一个Workflow的第一个step
      if (workflow_queue_.empty())
      {
        progress_bar_->hide();
        timer_->stop();
      }
      else
      {
        WorkflowConfig& workflow_config = workflow_queue_.front();
        if (workflow_config.step_queue.empty())
        {
        }
        WorkflowStepEntry& workflow_step_entry =
          workflow_config.step_queue.front();
        SetWorkflowStep(workflow_config.workflow_intermediate_directory,
                        workflow_step_entry);
      }
    }
  }
}

void BlocksPane::OnItemDoubleClicked(QTreeWidgetItem* item, int column)
{
  switch (item->type())
  {
  case BlocksTreeWidget::PHOTO_ORIENTATION:
    {
      ActivatePhotoOrientationItem(item);
      break;
    }
  case BlocksTreeWidget::POINT_CLOUD:
    {
      //ActivatePointCloudItem(item);
      break;
    }
  case BlocksTreeWidget::SURFACE_MODEL:
    {
      ActivateSurfaceModelItem(item);
      break;
    }
  }
}

void BlocksPane::OnActionAddBlockTriggered()
{
  typedef BlockPhotosSelectDialog::GroupEntryContainer GroupEntryContainer;

  db::RequestGetAllPhotogroups photogroups_request;
  db::ResponseGetAllPhotogroups photogroups_response;
  ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_ALL_PHOTOGROUPS,
    photogroups_request, photogroups_response, false);
  if (photogroups_response.error_code != db::DatabaseMediator::DATABASE_NO_ERROR)
    return;

  db::RequestGetAllPhotos photos_request;
  db::ResponseGetAllPhotos photos_response;
  ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_ALL_PHOTOS,
    photos_request, photos_response, false);
  if (photos_response.error_code != db::DatabaseMediator::DATABASE_NO_ERROR)
    return;

  GroupEntryContainer group_entries;
  auto itr_group_record = photogroups_response.records.begin();
  auto itr_group_record_end = photogroups_response.records.end();
  for (; itr_group_record != itr_group_record_end; ++itr_group_record)
  {
    PhotosTreeWidget::GroupEntry group_entry;
    group_entry.id = uint(itr_group_record->first);
    std::string name =
      itr_group_record->second[
        db::PhotogroupResource::PHOTOGROUP_FIELD_NAME].ToString();
    group_entry.name = QString::fromLocal8Bit(name.c_str());
    group_entries[group_entry.id] = group_entry;
  }

  auto itr_photo_record = photos_response.records.begin();
  auto itr_photo_record_end = photos_response.records.end();
  for (; itr_photo_record != itr_photo_record_end; ++itr_photo_record)
  {
    uint group_id =
      uint(itr_photo_record->second[
        db::PhotoResource::PHOTO_FIELD_PHOTOGROUP_ID].ToInt());
    auto itr_group_entry = group_entries.find(group_id);
    if (itr_group_entry == group_entries.end()) continue;

    PhotosTreeWidget::PhotoEntry photo_entry;
    photo_entry.id = uint(itr_photo_record->first);
    std::string photo_path =
      itr_photo_record->second[
        db::PhotoResource::PHOTO_FIELD_PATH].ToString();
    QFileInfo file_info(QString::fromLocal8Bit(photo_path.c_str()));
    photo_entry.file_name = file_info.fileName();
    photo_entry.x =
      PhotosTreeWidget::Float(
        itr_photo_record->second[
          db::PhotoResource::PHOTO_FIELD_POS_X].ToFloat());
    photo_entry.y =
      PhotosTreeWidget::Float(
        itr_photo_record->second[
          db::PhotoResource::PHOTO_FIELD_POS_Y].ToFloat());
    photo_entry.z =
      PhotosTreeWidget::Float(
        itr_photo_record->second[
          db::PhotoResource::PHOTO_FIELD_POS_Z].ToFloat());
    photo_entry.pitch =
      PhotosTreeWidget::Float(
        itr_photo_record->second[
          db::PhotoResource::PHOTO_FIELD_PITCH].ToFloat());
    photo_entry.roll =
      PhotosTreeWidget::Float(
        itr_photo_record->second[
          db::PhotoResource::PHOTO_FIELD_ROLL].ToFloat());
    photo_entry.heading =
      PhotosTreeWidget::Float(
        itr_photo_record->second[
          db::PhotoResource::PHOTO_FIELD_HEADING].ToFloat());
    itr_group_entry->second.photos[photo_entry.id] = photo_entry;
  }

  BlockPhotosSelectDialog::BlockInfoEntry block_info_entry;
  BlockPhotosSelectDialog dialog(block_info_entry, group_entries);
  if (dialog.exec())
  {
    //添加块
    block_info_entry = dialog.GetBlockInfo();
    hs::recon::db::RequestAddBlock request_add_block;
    hs::recon::db::ResponseAddBlock response_add_block;
    request_add_block.name = block_info_entry.name.toLocal8Bit().data();
    request_add_block.description =
      block_info_entry.description.toLocal8Bit().data();

    ((MainWindow*)parent())->database_mediator().Request(
      this, hs::recon::db::DatabaseMediator::REQUEST_ADD_BLOCK,
      request_add_block, response_add_block, true);
    if (response_add_block.error_code != db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QMessageBox msg_box;
      msg_box.setText(tr("Block name exists!"));
      msg_box.exec();
      return;
    }
    blocks_tree_widget_->AddBlock(uint(response_add_block.added_id),
                                  block_info_entry.name);

    //添加照片与块关系
    std::vector<uint> selected_photo_ids;
    dialog.GetSelectedPhotoIds(selected_photo_ids);
    hs::recon::db::RequestAddPhotosToBlock request_add_photos_to_block;
    hs::recon::db::ResponseAddPhotosToBlock response_add_photos_to_block;
    request_add_photos_to_block.block_id =
      hs::recon::db::Database::Identifier(response_add_block.added_id);
    auto itr_selected_photo_id = selected_photo_ids.begin();
    auto itr_selected_photo_id_end = selected_photo_ids.end();
    for (; itr_selected_photo_id != itr_selected_photo_id_end;
         ++itr_selected_photo_id)
    {
      request_add_photos_to_block.photo_ids.push_back(
        hs::recon::db::Database::Identifier(*itr_selected_photo_id));
    }
    ((MainWindow*)parent())->database_mediator().Request(
      this, hs::recon::db::DatabaseMediator::REQUEST_ADD_PHOTOS_TO_BLOCK,
      request_add_photos_to_block, response_add_photos_to_block, true);
    if (response_add_photos_to_block.error_code !=
        hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QMessageBox msg_box;
      msg_box.setText(tr("Add photos to block failed!"));
      msg_box.exec();
    }
    else
    {
      BlocksTreeWidget::StringMap photo_names;
      auto itr_added_photo = response_add_photos_to_block.photo_ids.begin();
      auto itr_added_photo_end = response_add_photos_to_block.photo_ids.end();
      for (; itr_added_photo != itr_added_photo_end; ++itr_added_photo)
      {
        hs::recon::db::RequestGetPhoto request_get_photo;
        hs::recon::db::ResponseGetPhoto response_get_photo;
        request_get_photo.id = *itr_added_photo;
        ((MainWindow*)parent())->database_mediator().Request(
          this, hs::recon::db::DatabaseMediator::REQUEST_GET_PHOTO,
          request_get_photo, response_get_photo, false);
        if (response_get_photo.error_code !=
            hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
        {
          QMessageBox msg_box;
          msg_box.setText(tr("Photo not exists!"));
          msg_box.exec();
          return;
        }
        std::string photo_path =
          response_get_photo.record[
            hs::recon::db::PhotoResource::PHOTO_FIELD_PATH].ToString();
        QFileInfo file_info(QString::fromLocal8Bit(photo_path.c_str()));
        photo_names[uint(*itr_added_photo)] = file_info.fileName();
      }
      blocks_tree_widget_->AddPhotosToBlock(uint(response_add_block.added_id),
                                            photo_names);
    }

  }
}

void BlocksPane::OnActionAddWorkflowTriggered()
{
  typedef hs::recon::db::Database::Identifier Identifier;
  int configure_type = 0;
  if (item_selected_mask_[BLOCK_SELECTED])
  {
    configure_type = WorkflowConfigureWidget::CONFIGURE_FEATURE_MATCH;
  }
  else if (item_selected_mask_[FEATURE_MATCH_SELECTED])
  {
    configure_type = WorkflowConfigureWidget::CONFIGURE_PHOTO_ORIENTATION;
  }
  else if (item_selected_mask_[PHOTO_ORIENTATION_SELECTED])
  {
    configure_type = WorkflowConfigureWidget::CONFIGURE_POINT_CLOUD;
  }
  else if (item_selected_mask_[POINT_CLOUD_SELECTED])
  {
    configure_type = WorkflowConfigureWidget::CONFIGURE_SURFACE_MODEL;
  }
  else if (item_selected_mask_[SURFACE_MODEL_SELECTED])
  {
    configure_type = WorkflowConfigureWidget::CONFIGURE_TEXTURE;
  }
  else
  {
    return;
  }

  WorkflowConfigureDialog workflow_configure_dialog(configure_type);
  if (workflow_configure_dialog.exec())
  {
    QSettings settings;
    QString intermediate_directory =
      settings.value(tr("intermediate_directory")).toString();
    WorkflowConfig workflow_config;
    workflow_config.workflow_intermediate_directory =
      intermediate_directory.toLocal8Bit().data();
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    workflow_config.workflow_intermediate_directory +=
      std::string("/") + boost::uuids::to_string(uuid) + "/";
    //workflow_config.workflow_intermediate_directory +=
    //  std::string("/69d6b682-6306-49f1-b8d9-604c08ec5f41/");

    //TODO: create_directories return false even that directories created!
    if (!boost::filesystem::create_directories(boost::filesystem::path(
          workflow_config.workflow_intermediate_directory)))
    {
      //QMessageBox msg_box;
      //msg_box.setText(tr("Fail to create intermediate directory."));
      //msg_box.exec();
      //return;
    }

    //添加feature match
    int first_configure_type = workflow_configure_dialog.FirstConfigureType();
    int last_configure_type = workflow_configure_dialog.LastConfigureType();
    uint block_id = selected_block_id_;
    uint feature_match_id = selected_feature_match_id_;
    uint photo_orientation_id = selected_photo_orientation_id_;
    uint point_cloud_id = selected_point_cloud_id_;
    uint surface_model_id = selected_surface_model_id_;
    bool add_feature_match = true;
    bool add_photo_orientation = true;
    bool add_point_cloud = true;
    bool add_surface_model = true;
    bool add_texture = true;
    if (first_configure_type !=
        WorkflowConfigureWidget::CONFIGURE_FEATURE_MATCH)
    {
      add_feature_match = false;
    }

    if (add_feature_match)
    {
      hs::recon::workflow::FeatureMatchConfigPtr feature_match_config(
        new hs::recon::workflow::FeatureMatchConfig);
      workflow_configure_dialog.FetchFeatureMatchConfig(*feature_match_config);
      if (AddFeatureMatchStep(block_id, feature_match_config,
                              workflow_config) != 0)
      {
        add_photo_orientation = false;
      }
      else
      {
        feature_match_id = workflow_config.step_queue.back().id;
      }
    }

    if (first_configure_type >
        WorkflowConfigureWidget::CONFIGURE_PHOTO_ORIENTATION ||
        last_configure_type <
        WorkflowConfigureWidget::CONFIGURE_PHOTO_ORIENTATION)
    {
      add_photo_orientation = false;
    }

    if (add_photo_orientation)
    {
      hs::recon::workflow::PhotoOrientationConfigPtr photo_orientation_config(
        new hs::recon::workflow::PhotoOrientationConfig);
      workflow_configure_dialog.FetchPhotoOrientationConfig(
        *photo_orientation_config);
      if (AddPhotoOrientationStep(feature_match_id, photo_orientation_config,
                                  workflow_config) != 0)
      {
        add_photo_orientation = false;
      }
      else
      {
        photo_orientation_id = workflow_config.step_queue.back().id;
      }
    }

    //添加点云
    if (first_configure_type >
      WorkflowConfigureWidget::CONFIGURE_POINT_CLOUD ||
      last_configure_type <
      WorkflowConfigureWidget::CONFIGURE_POINT_CLOUD)
    {
      add_point_cloud = false;
    }

    if (add_point_cloud)
    {
      hs::recon::workflow::PointCloudConfigPtr point_cloud_config(
        new hs::recon::workflow::PointCloudConfig);
      workflow_configure_dialog.FetchPointCloudConfig(
        *point_cloud_config);
      if (AddPointCloudStep(photo_orientation_id, point_cloud_config,
                            workflow_config) != 0)
      {
        add_point_cloud = false;
      }
      else
      {
        point_cloud_id = workflow_config.step_queue.back().id;
      }
    }

    //添加surface model
    if (first_configure_type
      > WorkflowConfigureWidget::CONFIGURE_SURFACE_MODEL ||
      last_configure_type
      < WorkflowConfigureWidget::CONFIGURE_SURFACE_MODEL)
    {
      add_surface_model = false;
    }
    if (add_surface_model)
    {
      hs::recon::workflow::MeshSurfaceConfigPtr surface_model_config(
        new hs::recon::workflow::MeshSurfaceConfig);
      workflow_configure_dialog.FetchSurfaceModelConfig(
        *surface_model_config);
      if (AddSurfaceModelStep(point_cloud_id
        , surface_model_config
        , workflow_config) != 0)
      {
        add_surface_model = false;
      }
      else
      {
        surface_model_id = workflow_config.step_queue.back().id;
      }

    }

    //添加texture
    if (first_configure_type
      > WorkflowConfigureWidget::CONFIGURE_TEXTURE ||
      last_configure_type
      < WorkflowConfigureWidget::CONFIGURE_TEXTURE)
    {
      add_texture = false;
    }
    if (add_texture)
    {
      hs::recon::workflow::TextureConfigPtr texture_config(
        new hs::recon::workflow::TextureConfig);
      workflow_configure_dialog.FetchTextureConfig(
        *texture_config);
      if (AddTextureStep(surface_model_id
        , texture_config
        , workflow_config) != 0)
      {
        add_texture = false;
      }

    }

    if (!workflow_config.step_queue.empty())
    {
      workflow_queue_.push(workflow_config);
      timer_->start(500);
    }
  }
}

void BlocksPane::OnActionCopyTriggered()
{
  if (item_selected_mask_[BLOCK_SELECTED])
  {
    db::RequestCopyBlock request;
    request.block_id = selected_block_id_;
    db::ResponseCopyBlock response;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_COPY_BLOCK,
      request, response, false);
    if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QString copied_block_name =
        QString::fromLocal8Bit(response.copied_block_name.c_str());
      blocks_tree_widget_->AddBlock(uint(response.copied_block_id),
                                    copied_block_name);
      BlocksTreeWidget::StringMap photo_names;
      for (const auto& photo_id : response.photo_ids)
      {
        hs::recon::db::RequestGetPhoto request_get_photo;
        hs::recon::db::ResponseGetPhoto response_get_photo;
        request_get_photo.id = db::Identifier(photo_id);
        ((MainWindow*)parent())->database_mediator().Request(
          this, hs::recon::db::DatabaseMediator::REQUEST_GET_PHOTO,
          request_get_photo, response_get_photo, false);
        if (response_get_photo.error_code !=
            hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
        {
          QMessageBox msg_box;
          msg_box.setText(tr("Photo not exists!"));
          msg_box.exec();
          return;
        }
        std::string photo_path =
          response_get_photo.record[
            hs::recon::db::PhotoResource::PHOTO_FIELD_PATH].ToString();
        QFileInfo file_info(QString::fromLocal8Bit(photo_path.c_str()));
        photo_names[uint(photo_id)] = file_info.fileName();
      }
      blocks_tree_widget_->AddPhotosToBlock(uint(response.copied_block_id),
                                            photo_names);
    }
  }
  else if (item_selected_mask_[FEATURE_MATCH_SELECTED])
  {
    db::RequestCopyFeatureMatch request;
    db::ResponseCopyFeatureMatch response;
    request.feature_match_id = selected_feature_match_id_;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_COPY_FEATURE_MATCH,
      request, response, false);
    if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QString copied_feature_name =
        QString::fromLocal8Bit(
          response.copied_feature_match_name.c_str());
      uint block_id = uint(response.block_id);
      uint feature_match_id = uint(response.copied_feature_match_id);
      blocks_tree_widget_->AddFeatureMatch(
        block_id, feature_match_id, copied_feature_name);
    }
  }
  else if (item_selected_mask_[PHOTO_ORIENTATION_SELECTED])
  {
    db::RequestCopyPhotoOrientation request;
    db::ResponseCopyPhotoOrientation response;
    request.photo_orientation_id = selected_photo_orientation_id_;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_COPY_FEATURE_MATCH,
      request, response, false);
    if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QString copied_feature_name =
        QString::fromLocal8Bit(
          response.copied_photo_orientation_name.c_str());
      uint feature_match_id = uint(response.feature_match_id);
      uint photo_orientation_id = uint(response.copied_photo_orientation_id);
      blocks_tree_widget_->AddPhotoOrientation(
        feature_match_id, photo_orientation_id, copied_feature_name);
    }
  }
  else if (item_selected_mask_[POINT_CLOUD_SELECTED])
  {
    db::RequestCopyPointCloud request;
    db::ResponseCopyPointCloud response;
    request.point_cloud_id = selected_point_cloud_id_;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_COPY_POINT_CLOUD,
      request, response, false);
    if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QString copied_feature_name =
        QString::fromLocal8Bit(
          response.copied_point_cloud_name.c_str());
      uint photo_orientation_id = uint(response.photo_orientation_id);
      uint point_cloud_id = uint(response.copied_point_cloud_id);
      blocks_tree_widget_->AddPointCloud(
        photo_orientation_id, point_cloud_id, copied_feature_name);
    }
  }
  else if (item_selected_mask_[SURFACE_MODEL_SELECTED])
  {
    db::RequestCopySurfaceModel request;
    db::ResponseCopySurfaceModel response;
    request.surface_model_id = selected_surface_model_id_;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_COPY_FEATURE_MATCH,
      request, response, false);
    if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QString copied_feature_name =
        QString::fromLocal8Bit(
          response.copied_surface_model_name.c_str());
      uint point_cloud_id = uint(response.point_cloud_id);
      uint surface_model_id = uint(response.copied_surface_model_id);
      blocks_tree_widget_->AddSurfaceModel(
        point_cloud_id, surface_model_id, copied_feature_name);
    }
  }
  else if (item_selected_mask_[TEXTURE_SELECTED])
  {
    db::RequestCopyTexture request;
    db::ResponseCopyTexture response;
    request.texture_id = selected_texture_id_;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_COPY_FEATURE_MATCH,
      request, response, false);
    if (response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      QString copied_feature_name =
        QString::fromLocal8Bit(
          response.copied_texture_name.c_str());
      uint surface_model_id = uint(response.surface_model_id);
      uint texture_id = uint(response.copied_texture_id);
      blocks_tree_widget_->AddTexture(
        surface_model_id, texture_id, copied_feature_name);
    }
  }
}

void BlocksPane::OnActionRemoveTriggered()
{
  QMessageBox msgBox;
  if(item_selected_mask_[BLOCK_SELECTED])
  {
     db::RequestRemoveBlock request;
     request.id = selected_block_id_;
     db::ResponseRemoveBlock response;
     ((MainWindow*)parent())->database_mediator().Request(
     this, db::DatabaseMediator::REQUEST_REMOVE_BLOCK,
     request, response, false);
     if(response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
     {
       blocks_tree_widget_->DeleteBlock(selected_block_id_);
     }
     else
     {
       msgBox.setText(tr("delete block fail!"));
       msgBox.exec();
     }
  }
  else if(item_selected_mask_[FEATURE_MATCH_SELECTED])
  {
    db::RequestRemoveFeatureMatch request;
    request.id = selected_feature_match_id_;
    db::ResponseRemoveFeatureMatch response;
    ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_REMOVE_FEATURE_MATCH,
    request, response, false);
    if(response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      blocks_tree_widget_->DeleteFeatureMatch(selected_feature_match_id_);
    }
    else
    {
      msgBox.setText(tr("delete Feature match fail!"));
      msgBox.exec();
    }
  }
  else if(item_selected_mask_[PHOTO_ORIENTATION_SELECTED])
  {
    db::RequestRemovePhotoOrientation request;
    request.id = selected_photo_orientation_id_;
    db::ResponseRemovePhotoOrientation response;
    ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_REMOVE_PHOTO_ORIENTATION,
    request, response, false);
    if(response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      blocks_tree_widget_->
        DeletePhotoOrientation(selected_photo_orientation_id_);
    }
    else
    {
      msgBox.setText(tr("delete Photo Orientation fail!"));
      msgBox.exec();
    }
  }
  else if(item_selected_mask_[POINT_CLOUD_SELECTED])
  {
    db::RequestRemoveSurfaceModel request;
    request.id = selected_point_cloud_id_;
    db::ResponseRemoveSurfaceModel response;
    ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_REMOVE_POINT_CLOUD,
    request, response, false);
    if(response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      blocks_tree_widget_->DeletePointCloud(selected_point_cloud_id_);
    }
    else
    {
      msgBox.setText(tr("delete point cloud fail!"));
      msgBox.exec();
    }
  }
  else if(item_selected_mask_[SURFACE_MODEL_SELECTED])
  {
    db::RequestRemoveSurfaceModel request;
    request.id = selected_surface_model_id_;
    db::ResponseRemoveSurfaceModel response;
    ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_REMOVE_SURFACE_MODEL,
    request, response, false);
    if(response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      blocks_tree_widget_->DeleteSurfaceModel(selected_surface_model_id_);
    }
    else
    {
      msgBox.setText(tr("delete surface model fail!"));
      msgBox.exec();
    }
  }
  else if(item_selected_mask_[TEXTURE_SELECTED])
  {
    db::RequestRemoveTextrue request;
    request.id = selected_texture_id_;
    db::ResponseRemoveTextrue response;
    ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_REMOVE_TEXTURE,
    request, response, false);
    if(response.error_code == db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      blocks_tree_widget_->DeleteTexture(selected_texture_id_);
    }
    else
    {
      msgBox.setText(tr("delete texture fail!"));
      msgBox.exec();
    }
  }
  else
  {
  return;
  }

}

void BlocksPane::OnActionInfoTriggered()
{
  if(item_selected_mask_[BLOCK_SELECTED])
  {
  }
  else if(item_selected_mask_[FEATURE_MATCH_SELECTED])
  {
  }
  else if(item_selected_mask_[PHOTO_ORIENTATION_SELECTED])
  {
    while(1)
    {
      typedef hs::recon::db::Database::Identifier Identifier;
      hs::recon::db::RequestGetPhotoOrientation request_photo_orientation;
      hs::recon::db::ResponseGetPhotoOrientation response_photo_orientation;
      request_photo_orientation.id = Identifier(selected_photo_orientation_id_);
      ((MainWindow*)parent())->database_mediator().Request(
        this, db::DatabaseMediator::REQUEST_GET_PHOTO_ORIENTATION,
        request_photo_orientation, response_photo_orientation, false);
      if(response_photo_orientation.error_code !=
          hs::recon::db::Database::DATABASE_NO_ERROR)
      {
        break;
      }

      if(response_photo_orientation.record[
        db::PhotoOrientationResource::PHOTO_ORIENTATION_FIELD_FLAG].ToInt()
        == db::PhotoOrientationResource::FLAG_NOT_COMPLETED)
      {
        break;
      }
        Identifier feature_match_id =
          int(response_photo_orientation.record[
            db::PhotoOrientationResource::
              PHOTO_ORIENTATION_FIELD_FEATURE_MATCH_ID].ToInt());
        hs::recon::db::RequestGetFeatureMatch request_feature_match;
        request_feature_match.id = feature_match_id;
        hs::recon::db::ResponseGetFeatureMatch response_feature_match;
        ((MainWindow*)parent())->database_mediator().Request(
          this, db::DatabaseMediator::REQUEST_GET_FEATURE_MATCH,
          request_feature_match, response_feature_match, false);
        if(response_feature_match.error_code !=
          db::DatabaseMediator::DATABASE_NO_ERROR)
        {
          break;
        }
        photo_orientation_info_widget_->Initialize(
          response_feature_match.keysets_path,
          response_photo_orientation.intrinsic_path,
          response_photo_orientation.extrinsic_path,
          response_photo_orientation.point_cloud_path,
          response_photo_orientation.tracks_path);
        photo_orientation_info_widget_->show();
        break;
    }
  }
  else if(item_selected_mask_[POINT_CLOUD_SELECTED])
  {
  }
  else if(item_selected_mask_[SURFACE_MODEL_SELECTED])
  {
  }
  else if(item_selected_mask_[TEXTURE_SELECTED])
  {
  }
}


void BlocksPane::OnBlockItemSelected(uint block_id)
{
  action_copy_->setEnabled(true);
  action_remove_->setEnabled(true);
  action_add_workflow_->setEnabled(true);
  action_info_->setEnabled(false);

  selected_block_id_ = block_id;
  item_selected_mask_.reset();
  item_selected_mask_.set(BLOCK_SELECTED);

  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::OnPhotosInOneBlockSelected(uint block_id,
                                            const std::vector<uint>& photo_ids)
{
  action_copy_->setEnabled(false);
  action_remove_->setEnabled(false);
  action_add_workflow_->setEnabled(false);
  action_info_->setEnabled(false);

  selected_block_id_ = block_id;
  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::OnPhotosItemSelected(uint block_id)
{
  action_copy_->setEnabled(false);
  action_remove_->setEnabled(false);
  action_add_workflow_->setEnabled(false);
  action_info_->setEnabled(false);

  selected_block_id_ = block_id;
  item_selected_mask_.reset();
  item_selected_mask_.set(PHOTOS_SELECTED);
  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::OnFeatureMatchItemSelected(uint feature_match_id)
{

  hs::recon::db::RequestGetFeatureMatch request_feature_match;
  hs::recon::db::ResponseGetFeatureMatch response_feature_match;
  request_feature_match.id = feature_match_id;
  ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_FEATURE_MATCH,
    request_feature_match, response_feature_match, false);

  if (response_feature_match.error_code == hs::recon::db::Database::DATABASE_NO_ERROR)
  {
    int flag = response_feature_match.record[
      db::FeatureMatchResource::FEATURE_MATCH_FIELD_FLAG].ToInt();
      if (flag == db::FeatureMatchResource::FLAG_NOT_COMPLETED)
      {
        action_copy_->setEnabled(false);
        action_add_workflow_->setEnabled(false);
      }
      else
      {
        action_copy_->setEnabled(true);
        action_add_workflow_->setEnabled(true);
      }
  }

  action_remove_->setEnabled(true);
  action_info_->setEnabled(false);

  selected_feature_match_id_ = feature_match_id;
  item_selected_mask_.reset();
  item_selected_mask_.set(FEATURE_MATCH_SELECTED);

  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::OnPhotoOrientationItemSelected(uint photo_orientation_id)
{
  db::RequestGetPhotoOrientation request_photo_orientation;
  db::ResponseGetPhotoOrientation response_photo_orientation;
  request_photo_orientation.id = photo_orientation_id;
  ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_PHOTO_ORIENTATION,
    request_photo_orientation, response_photo_orientation, false);

  if (response_photo_orientation.error_code == hs::recon::db::Database::DATABASE_NO_ERROR)
  {
    int flag = response_photo_orientation.record[
      db::PhotoOrientationResource::PHOTO_ORIENTATION_FIELD_FLAG].ToInt();
      if (flag == db::PhotoOrientationResource::FLAG_NOT_COMPLETED)
      {
        action_copy_->setEnabled(false);
        action_add_workflow_->setEnabled(false);
      }
      else
      {
        action_copy_->setEnabled(true);
        action_add_workflow_->setEnabled(true);
      }
  }

  action_remove_->setEnabled(true);
  action_info_->setEnabled(true);

  selected_photo_orientation_id_ = photo_orientation_id;
  item_selected_mask_.reset();
  item_selected_mask_.set(PHOTO_ORIENTATION_SELECTED);

  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::OnPointCloudItemSelected(uint point_cloud_id)
{
  db::RequestGetPointCloud request_point_cloud;
  db::ResponseGetPointCloud response_point_cloud;
  request_point_cloud.id = point_cloud_id;
  ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_POINT_CLOUD,
    request_point_cloud, response_point_cloud, false);
  if (response_point_cloud.error_code == hs::recon::db::Database::DATABASE_NO_ERROR)
  {
    int flag = response_point_cloud.record[
      db::PointCloudResource::POINT_CLOUD_FIELD_FLAG].ToInt();
      if (flag == db::PointCloudResource::FLAG_NOT_COMPLETED)
      {
        action_copy_->setEnabled(false);
        action_add_workflow_->setEnabled(false);
      }
      else
      {
        action_copy_->setEnabled(true);
        action_add_workflow_->setEnabled(true);
      }
  }

  action_remove_->setEnabled(true);
  action_info_->setEnabled(false);

  selected_point_cloud_id_ = point_cloud_id;
  item_selected_mask_.reset();
  item_selected_mask_.set(POINT_CLOUD_SELECTED);

  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::OnSurfaceModelItemSelected(uint surface_model_id)
{
  hs::recon::db::RequestGetSurfaceModel request_surface_model;
  hs::recon::db::ResponseGetSurfaceModel response_surface_model;
  request_surface_model.id = surface_model_id;
  ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_SURFACE_MODEL,
    request_surface_model, response_surface_model, false);
  if (response_surface_model.error_code ==hs::recon::db::Database::DATABASE_NO_ERROR)
  {
    int flag = response_surface_model.record[
      db::SurfaceModelResource::SURFACE_MODEL_FIELD_FLAG].ToInt();
      if (flag == db::SurfaceModelResource::FLAG_NOT_COMPLETED)
      {
        action_copy_->setEnabled(false);
        action_add_workflow_->setEnabled(false);
      }
      else
      {
        action_copy_->setEnabled(true);
        action_add_workflow_->setEnabled(true);
      }
  }

  action_remove_->setEnabled(true);
  action_info_->setEnabled(false);

  selected_surface_model_id_ = surface_model_id;
  item_selected_mask_.reset();
  item_selected_mask_.set(SURFACE_MODEL_SELECTED);
  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::OnTextureItemSelected(uint texture_id)
{
  hs::recon::db::RequestGetTexture request_texture;
  hs::recon::db::ResponseGetTexture response_texture;
  request_texture.id = texture_id;
  ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_TEXTURE,
    request_texture, response_texture, false);
  if (response_texture.error_code ==
    hs::recon::db::Database::DATABASE_NO_ERROR)
  {
    int flag = response_texture.record[
      db::TextureResource::TEXTURE_FIELD_FLAG].ToInt();
      if (flag == db::TextureResource::FLAG_NOT_COMPLETED)
      {
        action_copy_->setEnabled(false);
        action_add_workflow_->setEnabled(false);
      }
      else
      {
        action_copy_->setEnabled(true);
        action_add_workflow_->setEnabled(true);
      }
  }
  action_remove_->setEnabled(true);
  action_info_->setEnabled(false);

  selected_texture_id_ = texture_id;
  item_selected_mask_.reset();
  item_selected_mask_.set(TEXTURE_SELECTED);

  //Hide Photo Orientation information
  photo_orientation_info_widget_->hide();

}

void BlocksPane::ActivatePhotoOrientationItem(
  QTreeWidgetItem* photo_orientation_item)
{
  //清除point_cloud_item颜色
  QTreeWidgetItem* last_point_cloud_item =
    blocks_tree_widget_->PointCloudItem(activated_point_cloud_id_);
  if (last_point_cloud_item)
  {
    last_point_cloud_item->setBackground(0, backup_background_);
    activated_point_cloud_id_ = std::numeric_limits<uint>::max();
  }
  //清除surface_model_item颜色
  QTreeWidgetItem* last_surface_model_item =
    blocks_tree_widget_->SurfaceModelItem(activated_surface_model_id_);
  if (last_surface_model_item)
  {
    last_surface_model_item->setBackground(0, backup_background_);
    activated_surface_model_id_ = std::numeric_limits<uint>::max();
  }

  //设置photo_orientation_item颜色
  QTreeWidgetItem* last_photo_orientation_item =
    blocks_tree_widget_->PhotoOrientationItem(activated_photo_orientation_id_);
  if (last_photo_orientation_item == photo_orientation_item) return;
  if (last_photo_orientation_item)
  {
    last_photo_orientation_item->setBackground(0, backup_background_);
  }

  backup_background_ = photo_orientation_item->background(0);
  photo_orientation_item->setBackground(0, QBrush(QColor(200, 110, 90)));
  activated_photo_orientation_id_ =
    photo_orientation_item->data(0, Qt::UserRole).toUInt();
  emit PhotoOrientationActivated(activated_photo_orientation_id_);

}

void BlocksPane::ActivatePointCloudItem(
  QTreeWidgetItem* point_cloud_item)
{
  //清除photo_orientation_item颜色
  QTreeWidgetItem* last_photo_orientation_item =
    blocks_tree_widget_->PhotoOrientationItem(activated_photo_orientation_id_);
  if (last_photo_orientation_item)
  {
    last_photo_orientation_item->setBackground(0, backup_background_);
    activated_photo_orientation_id_ = std::numeric_limits<uint>::max();
  }
  //清除surface_model_item颜色
  QTreeWidgetItem* last_surface_model_item =
    blocks_tree_widget_->SurfaceModelItem(activated_surface_model_id_);
  if(last_surface_model_item)
  {
    last_surface_model_item->setBackground(0, backup_background_);
    activated_surface_model_id_ = std::numeric_limits<uint>::max();
  }

  //设置point_cloud_item颜色
  QTreeWidgetItem* last_point_cloud_item =
    blocks_tree_widget_->PointCloudItem(activated_point_cloud_id_);
  if (last_point_cloud_item == point_cloud_item) return;
  if (last_point_cloud_item)
  {
    last_point_cloud_item->setBackground(0, backup_background_);
  }

  backup_background_ = point_cloud_item->background(0);
  point_cloud_item->setBackground(0, QBrush(QColor(200, 110, 90)));
  activated_point_cloud_id_ =
    point_cloud_item->data(0, Qt::UserRole).toUInt();
  //emit PointCloudActivated(activated_point_cloud_id_);

}

void BlocksPane::ActivateSurfaceModelItem(QTreeWidgetItem* surface_model_item)
{
  //清除photo_orientation_item颜色
  QTreeWidgetItem* last_photo_orientation_item =
    blocks_tree_widget_->PhotoOrientationItem(activated_photo_orientation_id_);
  if(last_photo_orientation_item)
  {
    last_photo_orientation_item->setBackground(0, backup_background_);
    activated_photo_orientation_id_ = std::numeric_limits<uint>::max();
  }

  //清除point_cloud_item颜色
  QTreeWidgetItem* last_point_cloud_item =
    blocks_tree_widget_->PhotoOrientationItem(activated_point_cloud_id_);
  if(last_point_cloud_item)
  {
    last_point_cloud_item->setBackground(0, backup_background_);
    activated_point_cloud_id_ = std::numeric_limits<uint>::max();
  }

  //设置surface_model_item颜色
  QTreeWidgetItem* last_surface_model_item =
    blocks_tree_widget_->SurfaceModelItem(activated_surface_model_id_);
  if(last_surface_model_item == surface_model_item) return;
  if(last_surface_model_item)
  {
    last_surface_model_item->setBackground(0, backup_background_);
  }

  backup_background_ = surface_model_item->background(0);
  surface_model_item->setBackground(0, QBrush(QColor(200, 110, 90)));
  activated_surface_model_id_ =
    surface_model_item->data(0, Qt::UserRole).toUInt();
  emit SurfaceModelActivated(activated_surface_model_id_);

}

int BlocksPane::SetWorkflowStep(
  const std::string& workflow_intermediate_directory,
  WorkflowStepEntry& workflow_step_entry)
{
  switch (workflow_step_entry.config->type())
  {
  case workflow::STEP_FEATURE_MATCH:
    {
      current_step_ = SetFeatureMatchStep(workflow_intermediate_directory,
                                          workflow_step_entry);
      break;
    }
  case workflow::STEP_PHOTO_ORIENTATION:
    {
      current_step_ = SetPhotoOrientationStep(workflow_intermediate_directory,
                                              workflow_step_entry);
      break;
    }
  case workflow::STEP_POINT_CLOUD:
    {
      current_step_ = SetPointCloudStep(workflow_intermediate_directory,
                                        workflow_step_entry);
      break;
    }
  case workflow::STEP_SURFACE_MODEL:
    {
      current_step_ = SetSurfaceModelStep(workflow_intermediate_directory,
                                          workflow_step_entry);
      break;
    }
  case workflow::STEP_TEXTURE:
    {
      current_step_ = SetTextureStep(workflow_intermediate_directory,
                                     workflow_step_entry);
      break;
    }
  }
  return 0;
}

int BlocksPane::AddFeatureMatchStep(
  uint block_id,
  workflow::FeatureMatchConfigPtr feature_match_config,
  WorkflowConfig& workflow_config)
{
  //添加feature match
  typedef db::Database::Identifier Identifier;
  hs::recon::db::RequestAddFeatureMatch request_add_feature_match;
  hs::recon::db::ResponseAddFeatureMatch response_add_feature_match;
  request_add_feature_match.block_id = Identifier(block_id);
  ((MainWindow*)parent())->database_mediator().Request(
    this, hs::recon::db::DatabaseMediator::REQUEST_ADD_FEATURE_MATCH,
    request_add_feature_match, response_add_feature_match, true);
  if (response_add_feature_match.error_code ==
      hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
  {
    uint feature_match_id = uint(response_add_feature_match.feature_match_id);
    QString feature_match_name =
      QString::fromLocal8Bit(response_add_feature_match.name.c_str());
    blocks_tree_widget_->AddFeatureMatch(block_id,
                                         feature_match_id,
                                         feature_match_name);
    QTreeWidgetItem* feature_match_item =
      blocks_tree_widget_->FeatureMatchItem(feature_match_id);
    if (feature_match_item)
    {
      feature_match_item->setTextColor(0, Qt::gray);
    }
    WorkflowStepEntry feature_match_step_entry;
    feature_match_step_entry.id = uint(feature_match_id);
    feature_match_step_entry.config = feature_match_config;
    workflow_config.step_queue.push(feature_match_step_entry);

    return 0;
  }
  else
  {
    return -1;
  }
}

int BlocksPane::AddPhotoOrientationStep(
  uint feature_match_id,
  workflow::PhotoOrientationConfigPtr photo_orientation_config,
  WorkflowConfig& workflow_config)
{
  //添加photo orientation
  typedef db::Database::Identifier Identifier;
  hs::recon::db::RequestAddPhotoOrientation request_add_photo_orientation;
  hs::recon::db::ResponseAddPhotoOrientation response_add_photo_orientation;
  request_add_photo_orientation.feature_match_id =
    Identifier(feature_match_id);
  ((MainWindow*)parent())->database_mediator().Request(
    this, hs::recon::db::DatabaseMediator::REQUEST_ADD_PHOTO_ORIENTATION,
    request_add_photo_orientation, response_add_photo_orientation, true);
  if (response_add_photo_orientation.error_code ==
      hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
  {
    uint photo_orientation_id =
      uint(response_add_photo_orientation.photo_orientation_id);
    QString photo_orientation_name =
      QString::fromLocal8Bit(response_add_photo_orientation.name.c_str());
    blocks_tree_widget_->AddPhotoOrientation(feature_match_id,
                                             photo_orientation_id,
                                             photo_orientation_name);
    QTreeWidgetItem* photo_orientation_item =
      blocks_tree_widget_->PhotoOrientationItem(photo_orientation_id);
    if (photo_orientation_item)
    {
      photo_orientation_item->setTextColor(0, Qt::gray);
    }
    WorkflowStepEntry photo_orientation_step_entry;
    photo_orientation_step_entry.id = photo_orientation_id;
    photo_orientation_step_entry.config = photo_orientation_config;
    workflow_config.step_queue.push(photo_orientation_step_entry);
    return 0;
  }
  else
  {
    return -1;
  }
}

int BlocksPane::AddPointCloudStep(
  uint photo_orientation_id,
  workflow::PointCloudConfigPtr point_cloud_config,
  WorkflowConfig& workflow_config)
{
  typedef db::Database::Identifier Identifier;
  hs::recon::db::RequestAddPointCloud request_add_point_cloud;
  hs::recon::db::ResponseAddPointCloud response_add_point_cloud;
  request_add_point_cloud.photo_orientation_id =
    Identifier(photo_orientation_id);
  ((MainWindow*)parent())->database_mediator().Request(
    this, hs::recon::db::DatabaseMediator::REQUEST_ADD_POINT_CLOUD,
    request_add_point_cloud, response_add_point_cloud, true);
  if (response_add_point_cloud.error_code ==
    hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
  {
    uint point_cloud_id =
      uint(response_add_point_cloud.point_cloud_id);
    QString point_cloud_name =
      QString::fromLocal8Bit(response_add_point_cloud.name.c_str());
    blocks_tree_widget_->AddPointCloud(photo_orientation_id,
                                       point_cloud_id,
                                       point_cloud_name);
    QTreeWidgetItem* point_cloud_item =
      blocks_tree_widget_->PointCloudItem(point_cloud_id);
    if (point_cloud_item)
    {
      point_cloud_item->setTextColor(0, Qt::gray);
    }
    WorkflowStepEntry point_cloud_step_entry;
    point_cloud_step_entry.id = point_cloud_id;
    point_cloud_step_entry.config = point_cloud_config;
    workflow_config.step_queue.push(point_cloud_step_entry);
    return 0;
  }
  else
  {
    return -1;
  }
}

int BlocksPane::AddSurfaceModelStep(
  uint point_cloud_id,
  workflow::MeshSurfaceConfigPtr surface_model_confg,
  WorkflowConfig& workflow_config)
{
  typedef db::Database::Identifier Identifier;
  hs::recon::db::RequestAddSurfaceModel request_add_surface_model;
  hs::recon::db::ResponseAddSurfaceModel response_add_surface_model;
  request_add_surface_model.point_cloud_id = Identifier(point_cloud_id);
  ((MainWindow*)parent())->database_mediator().Request(
    this, hs::recon::db::DatabaseMediator::REQUEST_ADD_SURFACE_MODEL,
    request_add_surface_model, response_add_surface_model, true);
  if (response_add_surface_model.error_code ==
    hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
  {
    uint surface_model_id = uint(response_add_surface_model.surface_model_id);
    QString surface_model_name =
      QString::fromLocal8Bit(response_add_surface_model.name.c_str());
    blocks_tree_widget_->AddSurfaceModel(point_cloud_id,
      surface_model_id,
      surface_model_name);
    QTreeWidgetItem* surface_model_item =
      blocks_tree_widget_->SurfaceModelItem(surface_model_id);
    if (surface_model_item)
    {
      surface_model_item->setTextColor(0, Qt::gray);
    }
    WorkflowStepEntry surface_model_step_entry;
    surface_model_step_entry.id = uint(surface_model_id);
    surface_model_step_entry.config = surface_model_confg;
    workflow_config.step_queue.push(surface_model_step_entry);
    return 0;
  }
  else
  {
    return -1;
  }
}

int BlocksPane::AddTextureStep(
  uint surface_model_id,
  workflow::TextureConfigPtr texture_config,
  WorkflowConfig& workflow_config)
{
  typedef db::Database::Identifier Identifier;
  hs::recon::db::RequestAddTexture request_add_texture;
  hs::recon::db::ResponseAddTexture response_add_texture;
  request_add_texture.surface_model_id = Identifier(surface_model_id);
  ((MainWindow*)parent())->database_mediator().Request(
    this, hs::recon::db::DatabaseMediator::REQUEST_ADD_TEXTURE,
    request_add_texture, response_add_texture, true);
  if (response_add_texture.error_code ==
    hs::recon::db::DatabaseMediator::DATABASE_NO_ERROR)
  {
    uint texture_id = uint(response_add_texture.texture_id);
    QString texture_name =
      QString::fromLocal8Bit(response_add_texture.name.c_str());
    blocks_tree_widget_->AddTexture(surface_model_id,
                                    texture_id,
                                    texture_name);
    QTreeWidgetItem* texture_item =
      blocks_tree_widget_->TextureItem(texture_id);
    if (texture_item)
    {
      texture_item->setTextColor(0, Qt::gray);
    }
    WorkflowStepEntry texture_step_entry;
    texture_step_entry.id = uint(texture_id);
    texture_step_entry.config = texture_config;
    workflow_config.step_queue.push(texture_step_entry);
    return 0;
  }
  else
  {
    return -1;
  }
}

BlocksPane::WorkflowStepPtr BlocksPane::SetFeatureMatchStep(
  const std::string& workflow_intermediate_directory,
  WorkflowStepEntry& workflow_step_entry)
{
  typedef hs::recon::db::Database::Identifier Identifier;
  typedef hs::recon::workflow::FeatureMatchConfig::PosEntry PosEntry;
  typedef DefaultLongitudeLatitudeConvertor::CoordinateSystem
          CoordinateSystem;
  typedef CoordinateSystem::Projection Projection;
  typedef DefaultLongitudeLatitudeConvertor::Scalar Scalar;
  typedef DefaultLongitudeLatitudeConvertor::Convertor Convertor;
  typedef DefaultLongitudeLatitudeConvertor::Coordinate Coordinate;
  typedef DefaultLongitudeLatitudeConvertor::CoordinateContainer
          CoordinateContainer;
  typedef hs::cartographics::format::HS_FormatterProj4<Scalar> Formatter;

  while (1)
  {

    //获取特征匹配数据
    hs::recon::db::RequestGetFeatureMatch request_feature_match;
    hs::recon::db::ResponseGetFeatureMatch response_feature_match;
    request_feature_match.id = Identifier(workflow_step_entry.id);
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_FEATURE_MATCH,
      request_feature_match, response_feature_match, false);

    if (response_feature_match.error_code != hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }

    //获取特征匹配所在块包含的影像
    Identifier block_id =
      Identifier(
        response_feature_match.record[
          db::FeatureMatchResource::FEATURE_MATCH_FIELD_BLOCK_ID].ToInt());

    hs::recon::db::RequestGetPhotosInBlock request_get_photos_in_block;
    hs::recon::db::ResponseGetPhotosInBlock response_get_photos_in_block;
    request_get_photos_in_block.block_id = block_id;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_PHOTOS_IN_BLOCK,
      request_get_photos_in_block, response_get_photos_in_block, false);

    if (response_get_photos_in_block.error_code !=
        hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    auto itr_photo = response_get_photos_in_block.records.begin();
    auto itr_photo_end = response_get_photos_in_block.records.end();
    std::map<size_t, std::string> image_paths;
    std::map<size_t, std::string> descriptor_paths;
    std::map<size_t, PosEntry> pos_entries;
    double invalid_value = -1e-100;
    CoordinateSystem coordinate_system;
    for (size_t i = 0; itr_photo != itr_photo_end; ++itr_photo, i++)
    {
      size_t image_id = size_t(itr_photo->first);
      image_paths.insert(std::make_pair(
        image_id,
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_PATH].ToString()));
      descriptor_paths.insert(std::make_pair(
        image_id,
        boost::str(boost::format("%1%%2%.desc") %
                   workflow_intermediate_directory %
                   itr_photo->first)));
      PosEntry pos_entry;
      pos_entry.x =
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_POS_X].ToFloat();
      pos_entry.y =
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_POS_Y].ToFloat();
      pos_entry.z =
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_POS_Z].ToFloat();
      std::string coordinate_system_format =
        itr_photo->second[
          db::PhotoResource::PHOTO_FIELD_COORDINATE_SYSTEM].ToString();
      Formatter formatter;
      formatter.StringToCoordinateSystem(coordinate_system_format,
                                         coordinate_system);
      if (pos_entry.x > invalid_value &&
          pos_entry.y > invalid_value &&
          pos_entry.z > invalid_value)
      {
        pos_entries.insert(std::make_pair(image_id, pos_entry));
      }
    }

    //转换pos坐标系
    if (coordinate_system.projection().projection_type() ==
        Projection::TYPE_LAT_LONG)
    {
      CoordinateContainer coordinates_lat_long;
      auto itr_pos_entry = pos_entries.begin();
      auto itr_pos_entry_end = pos_entries.end();
      for (; itr_pos_entry != itr_pos_entry_end; ++itr_pos_entry)
      {
        Coordinate coordinate_lat_long;
        coordinate_lat_long<<itr_pos_entry->second.x,
                             itr_pos_entry->second.y,
                             itr_pos_entry->second.z;
        coordinates_lat_long.push_back(coordinate_lat_long);
      }
      DefaultLongitudeLatitudeConvertor default_convertor;
      CoordinateSystem coordinate_system_cartisian;
      default_convertor.GetDefaultCoordinateSystem(
        coordinate_system, coordinates_lat_long, coordinate_system_cartisian);

      Convertor convertor;
      itr_pos_entry = pos_entries.begin();
      for (; itr_pos_entry != itr_pos_entry_end; ++itr_pos_entry)
      {
        Coordinate coordinate_lat_long;
        coordinate_lat_long<<itr_pos_entry->second.x,
                             itr_pos_entry->second.y,
                             itr_pos_entry->second.z;
        Coordinate coordinate_cartisian;
        convertor.CoordinateSystemToCoordinateSystem(
          coordinate_system, coordinate_system_cartisian,
          coordinate_lat_long, coordinate_cartisian);
        itr_pos_entry->second.x = coordinate_cartisian[0];
        itr_pos_entry->second.y = coordinate_cartisian[1];
        itr_pos_entry->second.z = coordinate_cartisian[2];
      }
    }

    QSettings settings;
    QString number_of_threads_key = tr("number_of_threads");
    uint number_of_threads = settings.value(number_of_threads_key,
                                            QVariant(uint(1))).toUInt();
    workflow::FeatureMatchConfigPtr feature_match_config =
      std::static_pointer_cast<workflow::FeatureMatchConfig>(
        workflow_step_entry.config);
    feature_match_config->set_image_paths(image_paths);
    feature_match_config->set_keysets_path(
      response_feature_match.keysets_path);
    feature_match_config->set_descripor_paths(descriptor_paths);
    feature_match_config->set_pos_entries(pos_entries);
    feature_match_config->set_matches_path(
      response_feature_match.matches_path);
    feature_match_config->set_number_of_threads(int(number_of_threads));

    break;
  }
  return WorkflowStepPtr(new workflow::OpenCVFeatureMatch);
  //return WorkflowStepPtr(new workflow::OpenMVGFeatureMatch);
}

BlocksPane::WorkflowStepPtr BlocksPane::SetPhotoOrientationStep(
  const std::string& workflow_intermediate_directory,
  WorkflowStepEntry& workflow_step_entry)
{
  typedef hs::recon::db::Database::Identifier Identifier;
  typedef workflow::PhotoOrientationConfig::IntrinsicParams
          IntrinsicParams;
  typedef workflow::PhotoOrientationConfig::IntrinsicParamsContainer
          IntrinsicParamsContainer;
  typedef workflow::PhotoOrientationConfig::PosEntry PosEntry;
  typedef workflow::PhotoOrientationConfig::PosEntryContainer PosEntryContainer;
  typedef DefaultLongitudeLatitudeConvertor::CoordinateSystem
          CoordinateSystem;
  typedef CoordinateSystem::Projection Projection;
  typedef DefaultLongitudeLatitudeConvertor::Scalar Scalar;
  typedef DefaultLongitudeLatitudeConvertor::Convertor Convertor;
  typedef DefaultLongitudeLatitudeConvertor::Coordinate Coordinate;
  typedef DefaultLongitudeLatitudeConvertor::CoordinateContainer
          CoordinateContainer;
  typedef hs::cartographics::format::HS_FormatterProj4<Scalar> Formatter;

  while (1)
  {
    //获取相机朝向数据
    hs::recon::db::RequestGetPhotoOrientation request_photo_orientation;
    hs::recon::db::ResponseGetPhotoOrientation response_photo_orientation;
    request_photo_orientation.id = Identifier(workflow_step_entry.id);
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_PHOTO_ORIENTATION,
      request_photo_orientation, response_photo_orientation, false);
    if (response_photo_orientation.error_code !=
        hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }

    //TODO:Need to modify to portable.
    std::string photo_orientation_path =
      ((MainWindow*)parent())->database_mediator().GetPhotoOrientationPath(
        request_photo_orientation.id);

    //获取特征匹配数据
    int field_id =
      db::PhotoOrientationResource::PHOTO_ORIENTATION_FIELD_FEATURE_MATCH_ID;
    Identifier feature_match_id =
      Identifier(
        response_photo_orientation.record[field_id].ToInt());
    hs::recon::db::RequestGetFeatureMatch request_feature_match;
    hs::recon::db::ResponseGetFeatureMatch response_feature_match;
    request_feature_match.id = feature_match_id;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_FEATURE_MATCH,
      request_feature_match, response_feature_match, false);

    //获取特征匹配所在块包含的影像
    Identifier block_id =
      Identifier(
        response_feature_match.record[
          db::FeatureMatchResource::FEATURE_MATCH_FIELD_BLOCK_ID].ToInt());

    hs::recon::db::RequestGetPhotosInBlock request_get_photos_in_block;
    hs::recon::db::ResponseGetPhotosInBlock response_get_photos_in_block;
    request_get_photos_in_block.block_id = block_id;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_PHOTOS_IN_BLOCK,
      request_get_photos_in_block, response_get_photos_in_block, false);

    if (response_get_photos_in_block.error_code !=
        hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    auto itr_photo = response_get_photos_in_block.records.begin();
    auto itr_photo_end = response_get_photos_in_block.records.end();
    size_t number_of_photos = response_get_photos_in_block.records.size();
    std::vector<std::string> image_paths;
    std::vector<int> image_ids;
    hs::sfm::ObjectIndexMap image_intrinsic_map(number_of_photos);
    IntrinsicParamsContainer intrinsic_params_set;
    std::vector<int> intrinsic_ids;
    double invalid_value = -1e-100;
    PosEntryContainer pos_entries;
    CoordinateSystem coordinate_system;
    for (size_t i = 0; itr_photo != itr_photo_end; ++itr_photo, i++)
    {
      image_paths.push_back(
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_PATH].ToString());
      image_ids.push_back(int(itr_photo->first));

      PosEntry pos_entry;
      pos_entry.x =
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_POS_X].ToFloat();
      pos_entry.y =
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_POS_Y].ToFloat();
      pos_entry.z =
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_POS_Z].ToFloat();
      std::string coordinate_system_format =
        itr_photo->second[
          db::PhotoResource::PHOTO_FIELD_COORDINATE_SYSTEM].ToString();
      Formatter formatter;
      formatter.StringToCoordinateSystem(coordinate_system_format,
                                         coordinate_system);
      if (pos_entry.x > invalid_value &&
          pos_entry.y > invalid_value &&
          pos_entry.z > invalid_value)
      {
        pos_entries[i] = pos_entry;
      }

      int photogroup_id =
        itr_photo->second[db::PhotoResource::PHOTO_FIELD_PHOTOGROUP_ID].ToInt();
      bool found = false;
      for (size_t j = 0; j < intrinsic_ids.size(); j++)
      {
        if (intrinsic_ids[j] == photogroup_id)
        {
          found = true;
          image_intrinsic_map.SetObjectId(i, j);
          break;
        }
      }
      if (!found)
      {
        intrinsic_ids.push_back(int(photogroup_id));
        image_intrinsic_map.SetObjectId(i, intrinsic_ids.size() - 1);
        //获取照片组数据
        hs::recon::db::RequestGetPhotogroup request_photogroup;
        hs::recon::db::ResponseGetPhotogroup response_photogroup;
        request_photogroup.id = Identifier(photogroup_id);
        ((MainWindow*)parent())->database_mediator().Request(
          this, db::DatabaseMediator::REQUEST_GET_PHOTOGROUP,
          request_photogroup, response_photogroup, false);
        double focal_length =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_FOCAL_LENGTH].ToFloat();
        double pixel_size =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_PIXEL_X_SIZE].ToFloat();
        double principal_x =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_PRINCIPAL_X].ToFloat();
        double principal_y =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_PRINCIPAL_Y].ToFloat();
        double radial1 =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_RADIAL1].ToFloat();
        double radial2 =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_RADIAL2].ToFloat();
        double radial3 =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_RADIAL3].ToFloat();
        double decentering1 =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_DECENTERING1].ToFloat();
        double decentering2 =
          response_photogroup.record[
            db::PhotogroupResource::PHOTOGROUP_FIELD_DECENTERING2].ToFloat();
        IntrinsicParams intrinsic_params(focal_length / pixel_size,
                                         0,
                                         principal_x,
                                         principal_y,
                                         1,
                                         radial1,
                                         radial2,
                                         radial3);
        intrinsic_params_set.push_back(intrinsic_params);

      }
    }

    //转换pos坐标系
    if (coordinate_system.projection().projection_type() ==
        Projection::TYPE_LAT_LONG)
    {
      CoordinateContainer coordinates_lat_long;
      auto itr_pos_entry = pos_entries.begin();
      auto itr_pos_entry_end = pos_entries.end();
      for (; itr_pos_entry != itr_pos_entry_end; ++itr_pos_entry)
      {
        Coordinate coordinate_lat_long;
        coordinate_lat_long<<itr_pos_entry->second.x,
                             itr_pos_entry->second.y,
                             itr_pos_entry->second.z;
        coordinates_lat_long.push_back(coordinate_lat_long);
      }
      DefaultLongitudeLatitudeConvertor default_convertor;
      CoordinateSystem coordinate_system_cartisian;
      default_convertor.GetDefaultCoordinateSystem(
        coordinate_system, coordinates_lat_long, coordinate_system_cartisian);

      Convertor convertor;
      itr_pos_entry = pos_entries.begin();
      for (; itr_pos_entry != itr_pos_entry_end; ++itr_pos_entry)
      {
        Coordinate coordinate_lat_long;
        coordinate_lat_long<<itr_pos_entry->second.x,
                             itr_pos_entry->second.y,
                             itr_pos_entry->second.z;
        Coordinate coordinate_cartisian;
        convertor.CoordinateSystemToCoordinateSystem(
          coordinate_system, coordinate_system_cartisian,
          coordinate_lat_long, coordinate_cartisian);
        itr_pos_entry->second.x = coordinate_cartisian[0];
        itr_pos_entry->second.y = coordinate_cartisian[1];
        itr_pos_entry->second.z = coordinate_cartisian[2];
      }
    }

    QSettings settings;
    QString number_of_threads_key = tr("number_of_threads");
    uint number_of_threads = settings.value(number_of_threads_key,
      QVariant(uint(1))).toUInt();
    workflow::PhotoOrientationConfigPtr photo_orientation_config =
      std::static_pointer_cast<workflow::PhotoOrientationConfig>(
        workflow_step_entry.config);
    photo_orientation_config->set_image_intrinsic_map(image_intrinsic_map);
    photo_orientation_config->set_matches_path(
      response_feature_match.matches_path);
    photo_orientation_config->set_image_paths(image_paths);
    photo_orientation_config->set_keysets_path(
      response_feature_match.keysets_path);
    photo_orientation_config->set_image_ids(image_ids);
    photo_orientation_config->set_intrinsic_params_set(intrinsic_params_set);
    photo_orientation_config->set_intrinsic_ids(intrinsic_ids);
    photo_orientation_config->set_intrinsic_path(
      response_photo_orientation.intrinsic_path);
    photo_orientation_config->set_extrinsic_path(
      response_photo_orientation.extrinsic_path);
    photo_orientation_config->set_point_cloud_path(
      response_photo_orientation.point_cloud_path);
    photo_orientation_config->set_tracks_path(
      response_photo_orientation.tracks_path);
    photo_orientation_config->set_similar_transform_path(
      response_photo_orientation.similar_transform_path);
    photo_orientation_config->set_workspace_path(
      response_photo_orientation.workspace_path);
    photo_orientation_config->set_number_of_threads(uint(number_of_threads));
    photo_orientation_config->set_pos_entries(pos_entries);

    break;
  }
  return WorkflowStepPtr(new workflow::IncrementalPhotoOrientation);
}

BlocksPane::WorkflowStepPtr BlocksPane::SetPointCloudStep(
  const std::string& workflow_intermediate_directory,
  WorkflowStepEntry& workflow_step_entry)
{
  typedef hs::recon::db::Database::Identifier Identifier;
  while (1)
  {

    workflow::PointCloudConfigPtr point_cloud_config =
      std::static_pointer_cast<workflow::PointCloudConfig>(
      workflow_step_entry.config);

    //获取photo
    hs::recon::db::RequestGetAllPhotos  request_get_all_photos;
    hs::recon::db::ResponseGetAllPhotos response_get_all_photos;
    ((MainWindow*)parent())->database_mediator().Request(
    this, db::DatabaseMediator::REQUEST_GET_ALL_PHOTOS,
    request_get_all_photos, response_get_all_photos, false);
    if(response_get_all_photos.error_code != hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    for(auto iter = response_get_all_photos.records.begin();
          iter != response_get_all_photos.records.end();
          ++iter)
    {
      point_cloud_config->photo_paths().insert(std::make_pair(
        iter->second[db::PhotoResource::PHOTO_FIELD_ID].ToInt(), 
        iter->second[db::PhotoResource::PHOTO_FIELD_PATH].ToString()));
    }

    //获取点云数据
    hs::recon::db::RequestGetPointCloud request_point_cloud;
    hs::recon::db::ResponseGetPointCloud response_point_cloud;
    request_point_cloud.id = Identifier(workflow_step_entry.id);
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_POINT_CLOUD,
      request_point_cloud, response_point_cloud, false);
    if (response_point_cloud.error_code !=
      hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    std::string point_cloud_path =
      ((MainWindow*)parent())->database_mediator().GetPointCloudPath(
        request_point_cloud.id);

    //获取照片定向数据
    hs::recon::db::RequestGetPhotoOrientation request_photo_orientation;
    hs::recon::db::ResponseGetPhotoOrientation response_photo_orientation;
    request_photo_orientation.id =
      db::Identifier(response_point_cloud.record[
        db::PointCloudResource::POINT_CLOUD_FIELD_PHOTO_ORIENTATION_ID].ToInt());
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_PHOTO_ORIENTATION,
      request_photo_orientation, response_photo_orientation, false);
    if (response_photo_orientation.error_code != db::DatabaseMediator::DATABASE_NO_ERROR)
    {
      break;
    }

    QSettings settings;
    QString number_of_threads_key = tr("number_of_threads");
    uint number_of_threads = settings.value(number_of_threads_key,
      QVariant(uint(1))).toUInt();

    point_cloud_config->set_workspace_path(point_cloud_path);
    point_cloud_config->set_photo_orientation_path(
      response_photo_orientation.workspace_path);
    point_cloud_config->set_intrinsic_path(
      response_photo_orientation.intrinsic_path);
    point_cloud_config->set_extrinsic_path(
      response_photo_orientation.extrinsic_path);
    point_cloud_config->set_sparse_point_cloud_path(
      response_photo_orientation.point_cloud_path);
    point_cloud_config->set_intermediate_path(workflow_intermediate_directory);
    point_cloud_config->set_s_number_of_threads(number_of_threads);
    break;
  }//while(1)
  return WorkflowStepPtr(new workflow::PointCloud);
}

BlocksPane::WorkflowStepPtr BlocksPane::SetSurfaceModelStep(
  const std::string& workflow_intermediate_directory,
  WorkflowStepEntry& workflow_step_entry)
{
  typedef hs::recon::db::Database::Identifier Identifier;
  while (1)
  {
    workflow::MeshSurfaceConfigPtr mesh_surface_config =
      std::static_pointer_cast<workflow::MeshSurfaceConfig>(
      workflow_step_entry.config);
    //获取Surface Model数据
    hs::recon::db::RequestGetSurfaceModel request_surface_model;
    hs::recon::db::ResponseGetSurfaceModel response_surface_model;
    request_surface_model.id = Identifier(workflow_step_entry.id);
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_SURFACE_MODEL,
      request_surface_model, response_surface_model, false);
    if (response_surface_model.error_code !=
      hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    std::string surface_model_path =
      ((MainWindow*)parent())->database_mediator().GetSurfaceModelPath(
        request_surface_model.id);

    //获取Point Cloud
    hs::recon::db::RequestGetPointCloud request_point_cloud;
    hs::recon::db::ResponseGetPointCloud response_point_cloud;
    request_point_cloud.id = 
      response_surface_model.record[
        db::SurfaceModelResource::SURFACE_MODEL_FIELD_POINT_CLOUD_ID].ToInt();
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_POINT_CLOUD,
      request_point_cloud, response_point_cloud, false);
    if (response_point_cloud.error_code !=
      hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    std::string point_cloud_path =
      ((MainWindow*)parent())->database_mediator().GetPointCloudPath(
        request_point_cloud.id);
    QSettings settings;
    QString number_of_threads_key = QString("number_of_threads");
    uint number_of_threads = settings.value(number_of_threads_key,
        QVariant(uint(1))).toUInt();

    mesh_surface_config->set_xml_path(workflow_intermediate_directory + "surface_model_input.xml");
    mesh_surface_config->set_core_use(number_of_threads);
    mesh_surface_config->set_output_dir(surface_model_path);
    mesh_surface_config->set_point_cloud_path(point_cloud_path + "dense_pointcloud.bin");
    break;
  }

  //return WorkflowStepPtr(new workflow::PoissonSurface);
  return WorkflowStepPtr(new workflow::DelaunaySurfaceModel);
}

BlocksPane::WorkflowStepPtr BlocksPane::SetTextureStep(
  const std::string& workflow_intermediate_directory,
  WorkflowStepEntry& workflow_step_entry)
{
  typedef db::Database::Identifier Identifier;
  typedef workflow::TextureConfig::Scalar Scalar;
  typedef workflow::TextureConfig::IntrinsicParams IntrinsicParams;
  typedef workflow::TextureConfig::ExtrinsicParams ExtrinsicParams;
  typedef workflow::TextureConfig::ImageParams ImageParams;
  typedef workflow::TextureConfig::ImageParamsContainer ImageParamsContainer;
  typedef hs::sfm::CameraIntrinsicParams<Scalar> IntrinsicParams;
  typedef EIGEN_STD_MAP(size_t, IntrinsicParams) IntrinsicParamsMap;
  typedef hs::sfm::CameraExtrinsicParams<Scalar> ExtrinsicParams;
  typedef std::pair<size_t, size_t> ExtrinsicIndex;
  typedef EIGEN_STD_MAP(ExtrinsicIndex, ExtrinsicParams) ExtrinsicParamsMap;

  while (1)
  {
    workflow::TextureConfigPtr texture_config =
      std::static_pointer_cast<workflow::TextureConfig>(
      workflow_step_entry.config);
    //获取Texture数据
    hs::recon::db::RequestGetTexture request_texture;
    hs::recon::db::ResponseGetTexture response_texture;
    request_texture.id = Identifier(workflow_step_entry.id);
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_TEXTURE,
      request_texture, response_texture, false);
    if (response_texture.error_code !=
      hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    std::string texture_path =
      ((MainWindow*)parent())->database_mediator().GetTexturePath(
        request_texture.id);

    //获取Surface Model
    hs::recon::db::RequestGetSurfaceModel request_surface_model;
    hs::recon::db::ResponseGetSurfaceModel response_surface_model;
    request_surface_model.id = 
      response_texture.record[
        db::TextureResource::TEXTURE_FIELD_SURFACE_MODEL_ID].ToInt();
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_SURFACE_MODEL,
      request_surface_model, response_surface_model, false);
    if (response_texture.error_code !=
      hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }
    std::string surface_model_path = response_surface_model.mesh_path;
    Identifier point_cloud_id =
      response_surface_model.record[
        db::SurfaceModelResource::SURFACE_MODEL_FIELD_POINT_CLOUD_ID].ToInt();

    //获取point cloud
    db::RequestGetPointCloud request_point_cloud;
    db::ResponseGetPointCloud response_point_cloud;
    request_point_cloud.id = point_cloud_id;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_POINT_CLOUD,
      request_point_cloud, response_point_cloud, false);
    if (response_point_cloud.error_code != hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }

    Identifier photo_orientation_id =
      response_point_cloud.record[
      db::PointCloudResource::POINT_CLOUD_FIELD_PHOTO_ORIENTATION_ID].ToInt();

    //获取photo orientation
    db::RequestGetPhotoOrientation request_photo_orientation;
    db::ResponseGetPhotoOrientation response_photo_orientation;
    request_photo_orientation.id = photo_orientation_id;
    ((MainWindow*)parent())->database_mediator().Request(
      this, db::DatabaseMediator::REQUEST_GET_PHOTO_ORIENTATION,
      request_photo_orientation, response_photo_orientation, false);
    if (response_photo_orientation.error_code !=
        hs::recon::db::Database::DATABASE_NO_ERROR)
    {
      break;
    }

    workflow::TextureConfig::SimilarTransform similar_transform;
    {
      std::ifstream similar_file(
        response_photo_orientation.similar_transform_path, std::ios::binary);
      if (!similar_file)
      {
        break;
      }
      cereal::PortableBinaryInputArchive archive(similar_file);
      archive(similar_transform.scale,
              similar_transform.rotation,
              similar_transform.translate);
    }

    IntrinsicParamsMap intrinsic_params_map;
    {
      std::ifstream intrinsic_file(
        response_photo_orientation.intrinsic_path, std::ios::binary);
      if (!intrinsic_file)
      {
        break;
      }
      cereal::PortableBinaryInputArchive archive(intrinsic_file);
      archive(intrinsic_params_map);
    }

    ImageParamsContainer images;
    ExtrinsicParamsMap extrinsic_params_map;
    {
      std::ifstream extrinsic_file(
        response_photo_orientation.extrinsic_path, std::ios::binary);
      if (!extrinsic_file)
      {
        break;
      }
      cereal::PortableBinaryInputArchive archive(extrinsic_file);
      archive(extrinsic_params_map);
    }

    bool miss_intrinsic = false;
    for (const auto& extrinsic_params : extrinsic_params_map)
    {
      ImageParams image;
      size_t image_id = extrinsic_params.first.first;
      size_t intrinsic_id = extrinsic_params.first.second;
      auto itr_intrinsic = intrinsic_params_map.find(intrinsic_id);
      if (itr_intrinsic == intrinsic_params_map.end())
      {
        miss_intrinsic = true;
        break;
      }
      image.intrinsic_params = itr_intrinsic->second;
      image.extrinsic_params = extrinsic_params.second;
      db::RequestGetPhotogroup request_group;
      db::ResponseGetPhotogroup response_group;
      request_group.id = db::Database::Identifier(intrinsic_id);
      ((MainWindow*)parent())->database_mediator().Request(
        this, db::DatabaseMediator::REQUEST_GET_PHOTOGROUP,
        request_group, response_group, false);
      if (response_group.error_code != db::DatabaseMediator::DATABASE_NO_ERROR)
      {
        continue;
      }
      db::RequestGetPhoto request_photo;
      db::ResponseGetPhoto response_photo;
      request_photo.id = db::Database::Identifier(image_id);
      ((MainWindow*)parent())->database_mediator().Request(
        this, db::DatabaseMediator::REQUEST_GET_PHOTO,
        request_photo, response_photo, false);
      if (response_photo.error_code != db::DatabaseMediator::DATABASE_NO_ERROR)
      {
        continue;
      }
      image.image_width =
        response_group.record[
          db::PhotogroupResource::PHOTOGROUP_FIELD_WIDTH].ToInt();
      image.image_height =
        response_group.record[
          db::PhotogroupResource::PHOTOGROUP_FIELD_HEIGHT].ToInt();
      image.image_path =
        response_photo.record[
          db::PhotoResource::PHOTO_FIELD_PATH].ToString();
      images.push_back(image);
    }
    if (miss_intrinsic)
    {
      break;
    }

    QSettings settings;
    QString number_of_threads_key = QString("number_of_threads");
    uint number_of_threads = settings.value(number_of_threads_key,
        QVariant(uint(1))).toUInt();

    texture_config->set_surface_model_path(surface_model_path);
    texture_config->set_similar_transform(similar_transform);
    texture_config->set_images(images);

    break;
  }

  return WorkflowStepPtr(new workflow::RoughTexture);
}

}
}
}
