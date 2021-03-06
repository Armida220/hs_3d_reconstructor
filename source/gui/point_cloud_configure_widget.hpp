﻿#ifndef _HS_3D_RECONSTRUCTOR_POINT_CLOUD_CONFIGURE_WIDGET_HPP_
#define _HS_3D_RECONSTRUCTOR_POINT_CLOUD_CONFIGURE_WIDGET_HPP_

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSpacerItem>

#include "workflow/point_cloud/pmvs_point_cloud.hpp"

namespace hs
{
namespace recon
{
namespace gui
{

class PointCloudConfigureWidget : public QWidget
{
  Q_OBJECT
public:
  PointCloudConfigureWidget(QWidget* parent = 0, Qt::WindowFlags f = 0);

  void FetchPointCloudConfig(
    workflow::PointCloudConfig& point_cloud_config);

private:
  void UsingSparsePointCloud();

  QVBoxLayout* layout_all_;
  QVBoxLayout* layout_inbox_;
  QGroupBox* group_box_;
  QComboBox* combo_box_;
  QCheckBox* using_sparse_point_cloud_;
  QLabel* label_point_cloud_quality_;
  QLabel* label_using_sparse_;
  QHBoxLayout* layout_point_cloud_quality_;
  QHBoxLayout* layout_using_sparse_;

};

}
}
}

#endif
