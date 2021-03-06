#include "gui/start_up_dialog.hpp"
#include "gui/main_window.hpp"

#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace hs
{
  namespace recon
  {
    namespace gui
    {

      StartUpDialog::StartUpDialog(QWidget* parent, Qt::WindowFlags f) 
        : QDialog(parent, f)
      {
        this->setWindowFlags(this->windowFlags() | Qt::FramelessWindowHint);
        layout_main_ = new QHBoxLayout;
        layout_group_quick_start_ = new QVBoxLayout;
        layout_group_recent_files_ = new QVBoxLayout;
        groupbox_quick_start_ = new QGroupBox(tr("Quick Start"));
        groupbox_recent_files_ = new QGroupBox(tr("Recent File"));
        button_create_project_ = new QPushButton(tr("&New Project..."), this);
        button_open_project_ = new QPushButton(tr("&Open Project..."), this);
        button_tools_ = new QPushButton(tr("&Tools..."), this);
        button_cancel_ = new QPushButton(tr("&Cancel"), this);

        for (int i = 0; i < MaxRecentFiles; ++i)
        {
          recent_file_acts_[i] = new QAction(this);
          recent_file_acts_[i]->setVisible(false);
          connect(recent_file_acts_[i], SIGNAL(triggered()),
            this, SLOT(OnRecentProjectClicked()));
          menu_recent_file_[i] = new QToolBar;
          menu_recent_file_[i]->addAction(recent_file_acts_[i]);
          layout_group_recent_files_->addWidget(menu_recent_file_[i]);
        }

        UpdateRecentFileActions();
        layout_main_->addWidget(groupbox_quick_start_);
        layout_main_->addWidget(groupbox_recent_files_);
        layout_group_quick_start_->addWidget(button_create_project_);
        layout_group_quick_start_->addWidget(button_open_project_);
        layout_group_quick_start_->addWidget(button_tools_);
        layout_group_quick_start_->addWidget(button_cancel_);
        groupbox_quick_start_->setLayout(layout_group_quick_start_);
        groupbox_recent_files_->setLayout(layout_group_recent_files_);
        this->setLayout(layout_main_);

        this->setWindowTitle(tr("Quick Start"));

        QObject::connect(button_create_project_, &QPushButton::clicked
          , this,&StartUpDialog::OnButtonCreateProjectClicked);
        QObject::connect(button_open_project_, &QPushButton::clicked
          , this, &StartUpDialog::OnButtonOpenProjectClicked);
        QObject::connect(button_cancel_, &QPushButton::clicked
          , this, &StartUpDialog::OnButtonCancelClicked);
        QObject::connect(button_tools_, &QPushButton::clicked
          , this, &StartUpDialog::OnButtonToolsClicked);
      }

      void StartUpDialog::OnButtonCreateProjectClicked()
      {
        NewProjcet();
      }

      void StartUpDialog::OnButtonOpenProjectClicked()
      {
        OpenProject();
      }

      void StartUpDialog::OnButtonToolsClicked()
      {
        OpenTools();
      }

      void StartUpDialog::OnButtonCancelClicked()
      {
        exit(0);
      }

      QString StartUpDialog::StrippedName(const QString &full_file_name)
      {
        return QFileInfo(full_file_name).fileName();
      }

      void StartUpDialog::SetCurrentFile(const QString &file_name)
      {
        QSettings settings;
        QStringList files = settings.value("recentFileList").toStringList();
        files.removeAll(file_name);
        QStringList not_exists_files;
        for (int i = 0; i != files.size(); i++)
        {
          std::string file = files[i].toLocal8Bit().data();
          if (!boost::filesystem::exists(file))
          {
            std::cout << file << std::endl;
            not_exists_files << files[i];
          }
        }

        for (int i = 0; i != not_exists_files.size(); i++)
        {
          files.removeAll(not_exists_files[i]);
        }

        files.prepend(file_name);
        while (files.size() > MaxRecentFiles)
        {
          files.removeLast();
        }

        settings.setValue("recentFileList", files);

        UpdateRecentFileActions();
      }

      void StartUpDialog::UpdateRecentFileActions()
      {
        QSettings settings;
        QStringList files = settings.value("recentFileList").toStringList();
        int num_recent_file = qMin(files.size(), (int)MaxRecentFiles);
        int i = 0;
        int j = 0;
        while (j<num_recent_file)
        {
          std::string str_file = files[j].toLocal8Bit().data();
          if (boost::filesystem::exists(str_file))
          {
            QString text = tr("&%1 %2").arg(i + 1).arg(StrippedName(files[i]));
            recent_file_acts_[i]->setText(text);
            recent_file_acts_[i]->setData(files[i]);
            recent_file_acts_[i]->setVisible(true);
            i++;
          }
          j++;
        }
      }

      void StartUpDialog::OnRecentProjectClicked()
      {
        QAction *action = qobject_cast<QAction *>(sender());
        if (action)
        {
          OpenProjectWithFile(action->data().toString());
        }
      }

      void StartUpDialog::keyPressEvent(QKeyEvent *e)
      {
        if (e->key() != Qt::Key_Escape)
        {
          QDialog::keyPressEvent(e);
        }
      }
    }
  }
}