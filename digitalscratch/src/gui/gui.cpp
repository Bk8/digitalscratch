/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch Player                           */
/*                                                                            */
/*                                                                            */
/*----------------------------------------------------------------( gui.cpp )-*/
/*                                                                            */
/*  Copyright (C) 2003-2014                                                   */
/*                Julien Rosener <julien.rosener@digital-scratch.org>         */
/*                                                                            */
/*----------------------------------------------------------------( License )-*/
/*                                                                            */
/*  This program is free software: you can redistribute it and/or modify      */
/*  it under the terms of the GNU General Public License as published by      */ 
/*  the Free Software Foundation, either version 3 of the License, or         */
/*  (at your option) any later version.                                       */
/*                                                                            */
/*  This package is distributed in the hope that it will be useful,           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/*  GNU General Public License for more details.                              */
/*                                                                            */
/*  You should have received a copy of the GNU General Public License         */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.      */
/*                                                                            */
/*------------------------------------------------------------( Description )-*/
/*                                                                            */
/*                Creates GUI for DigitalScratch player                       */
/*                                                                            */
/*============================================================================*/

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollBar>
#include <QHeaderView>
#include <QPushButton>
#include <QShortcut>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QtDebug>
#include <QtGlobal>
#include <iostream>
#include <QGraphicsScene>
#include <QFileSystemModel>
#include <samplerate.h>
#include <jack/jack.h>
#include <QSignalMapper>
#include <QMessageBox>
#include <QApplication>
#include <QPushButton>
#include <QCommandLinkButton>
#include <QSplitter>
#include <QModelIndexList>
#include <QAction>
#include <QMenu>
#include <QMimeData>
#include <QSizePolicy>
#include <QtConcurrentRun>
#include <math.h>

#include "application_logging.h"
#include "gui.h"
#include "digital_scratch_api.h"
#include "audio_collection_model.h"
#include "utils.h"
#include "singleton.h"
#include "keyfinder_api.h"
#include "playlist.h"
#include "playlist_persistence.h"

#ifdef WIN32
extern "C"
{
    #include "libavcodec/version.h"
    #include "libavformat/version.h"
}
#else
extern "C"
{
    #ifndef INT64_C
    #define INT64_C(c) (c ## LL)
    #define UINT64_C(c) (c ## ULL)
    #endif
    #include "libavcodec/version.h"
    #include "libavformat/version.h"
}
#endif

Gui::Gui(QList<QSharedPointer<Audio_track>>                        &in_ats,
         QList<QList<QSharedPointer<Audio_track>>>                 &in_at_samplers,
         QList<QSharedPointer<Audio_file_decoding_process>>        &in_decs,
         QList<QList<QSharedPointer<Audio_file_decoding_process>>> &in_dec_samplers,
         QList<QSharedPointer<Playback_parameters>>                &in_params,
         QSharedPointer<Audio_track_playback_process>              &in_playback,
         unsigned short int                                         in_nb_decks,
         QSharedPointer<Sound_driver_access_rules>                 &in_sound_card,
         QSharedPointer<Sound_capture_and_playback_process>        &in_capture_and_playback,
         int                                                       *in_dscratch_ids)
{
    // Check input parameters.
    if (in_playback.data()             == NULL ||
        in_sound_card.data()           == NULL ||
        in_capture_and_playback.data() == NULL ||
        in_dscratch_ids                == NULL)
    {
        qCCritical(DS_OBJECTLIFE) << "bad input parameters";
        return;
    }

    // Get decks/tracks and sound capture/playback engine.
    this->ats                     = in_ats;
    this->at_samplers             = in_at_samplers;
    this->nb_samplers             = 0;
    if (in_at_samplers.count() >= 1)
    {
        this->nb_samplers         = in_at_samplers[0].count();
    }
    this->decs                    = in_decs;
    this->dec_samplers            = in_dec_samplers;
    this->params                  = in_params;
    this->playback                = in_playback;
    this->nb_decks                = in_nb_decks;
    this->sound_card              = in_sound_card;
    this->capture_and_play        = in_capture_and_playback;
    this->dscratch_ids            = in_dscratch_ids;

    // Init pop-up dialogs.
    this->config_dialog                   = NULL;
    this->refresh_audio_collection_dialog = NULL;
    this->about_dialog                    = NULL;

    // Get app settings.
    this->settings = &Singleton<Application_settings>::get_instance();

    // Create and show the main window.
    if (this->create_main_window() != true)
    {
        qCCritical(DS_OBJECTLIFE) << "creation of main window failed";
        return;
    }

    // Apply application settings.
    if (this->apply_application_settings() != true)
    {
        qCCritical(DS_APPSETTINGS) << "can not apply application settings";
        return;
    }

    // Run motion detection if the setting auto_start_motion_detection=ON.
    if (this->settings->get_autostart_motion_detection() == true)
    {
        this->start_control_and_playback();
    }

    // Display audio file collection (takes time, that's why we are first showing the main window).
    this->display_audio_file_collection();

    return;
}

Gui::~Gui()
{
    // Store size/position of the main window (first go back from fullscreen or maximized mode).
    if (this->window->isFullScreen() == true)
    {
        this->window->showNormal();
    }
    this->settings->set_main_window_position(this->window->mapToGlobal(QPoint(0, 0)));
    this->settings->set_main_window_size(this->window->size());
    this->settings->set_browser_splitter_size(this->browser_splitter->saveState());

    // Cleanup.
    this->clean_header_buttons();
    this->clean_decks_area();
    this->clean_samplers_area();
    this->clean_file_browser_area();
    this->clean_bottom_help();
    this->clean_bottom_status();
    delete this->window;

    return;
}

bool
Gui::apply_application_settings()
{
    // Apply windows style.
    this->window_style = this->settings->get_gui_style();
    if (this->apply_main_window_style() != true)
    {
        qCWarning(DS_APPSETTINGS) << "cannot set new style to main window";
    }
    this->browser_splitter->restoreState(this->settings->get_browser_splitter_size());

    // Show/hide samplers.
    if (this->settings->get_samplers_visible() == true)
    {
        this->show_samplers();
    }
    else
    {
        this->hide_samplers();
    }

    // Change base path for tracks browser.
    if (this->file_system_model->get_root_path() != this->settings->get_tracks_base_dir_path())
    {
        this->set_folder_browser_base_path(this->settings->get_tracks_base_dir_path());
        if (this->is_window_rendered == true) // Do not do it if main window is not already displayed.
        {
            this->set_file_browser_base_path(this->settings->get_tracks_base_dir_path());
        }
    }

    // Apply motion detection settings for all turntables.
    for (int i = 0; i < this->nb_decks; i++)
    {
        if (dscratch_change_vinyl_type(this->dscratch_ids[i],
                                       (char*)this->settings->get_vinyl_type().toStdString().c_str()) != 0)
        {
            qCWarning(DS_APPSETTINGS) << "cannot set vinyl type";
        }
        if (dscratch_set_rpm(this->dscratch_ids[i], this->settings->get_rpm()) != 0)
        {
            qCWarning(DS_APPSETTINGS) << "cannot set turntable RPM";
        }
        if (dscratch_set_input_amplify_coeff(this->dscratch_ids[i], this->settings->get_input_amplify_coeff()) != 0)
        {
            qCWarning(DS_APPSETTINGS) << "cannot set new input amplify coeff value";
        }
        if (dscratch_set_min_amplitude_for_normal_speed(this->dscratch_ids[i], this->settings->get_min_amplitude_for_normal_speed()) != 0)
        {
            qCWarning(DS_APPSETTINGS) << "cannot set new min amplitude for normal speed value";
        }
        if (dscratch_set_min_amplitude(this->dscratch_ids[i], this->settings->get_min_amplitude()) != 0)
        {
            qCWarning(DS_APPSETTINGS) << "cannot set new min amplitude value";
        }
    }

    // Change shortcuts.
    this->shortcut_switch_playback->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_SWITCH_PLAYBACK)));
    this->shortcut_collapse_browser->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_COLLAPSE_BROWSER)));
    this->shortcut_load_audio_file->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_DECK)));
    this->shortcut_go_to_begin->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK)));
    this->shortcut_load_sample_file_1->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER1)));
    this->shortcut_load_sample_file_2->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER2)));
    this->shortcut_load_sample_file_3->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER3)));
    this->shortcut_load_sample_file_4->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER4)));
    this->shortcut_show_next_keys->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_SHOW_NEXT_KEYS)));
    this->shortcut_fullscreen->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_FULLSCREEN)));
    this->shortcut_help->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_HELP)));
    this->shortcut_file_search->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_FILE_SEARCH)));
    this->shortcut_file_search_press_enter->setKey(QKeySequence(Qt::Key_Enter));
    this->shortcut_file_search_press_esc->setKey(QKeySequence(Qt::Key_Escape));
    for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
    {
        this->shortcut_set_cue_points[i]->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_SET_CUE_POINTS_ON_DECK[i])));
        this->shortcut_go_to_cue_points[i]->setKey(QKeySequence(this->settings->get_keyboard_shortcut(KB_PLAY_CUE_POINTS_ON_DECK[i])));
    }

    // Set shortcut value in help bottom span.
    this->set_help_shortcut_value();

    return true;
}

void
Gui::start_control_and_playback()
{
    // Start sound card for capture and playback.
    if ((this->sound_card->is_running() == false) &&
        (this->sound_card->start((void*)this->capture_and_play.data()) == false))
    {
        qCWarning(DS_SOUNDCARD) << "can not start sound card";
        this->start_capture_button->setChecked(false);
        this->stop_capture_button->setChecked(true);
    }
    else
    {
        this->start_capture_button->setChecked(true);
        this->stop_capture_button->setChecked(false);
    }

    return;
}

void
Gui::stop_control_and_playback()
{
    // Stop sound card for capture and playback.
    if ((this->sound_card->is_running() == true) &&
        (this->can_stop_capture_and_playback() == true) &&
        (this->sound_card->stop() == false))
    {
        qCWarning(DS_SOUNDCARD) << "can not stop sound card";
        this->stop_capture_button->setChecked(false);
        this->start_capture_button->setChecked(true);
    }
    else
    {
        this->stop_capture_button->setChecked(true);
        this->start_capture_button->setChecked(false);
    }

    return;
}

bool
Gui::can_stop_capture_and_playback()
{
    // Show a pop-up asking to confirm to stop capture.
    QMessageBox msg_box;
    msg_box.setWindowTitle("DigitalScratch");
    msg_box.setText(tr("Do you really want to stop turntable motion detection ?"));
    msg_box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msg_box.setIcon(QMessageBox::Question);
    msg_box.setDefaultButton(QMessageBox::Cancel);
    msg_box.setStyleSheet(Utils::get_current_stylesheet_css());
    if (this->nb_decks > 1)
    {
        msg_box.setWindowIcon(QIcon(ICON_2));
    }
    else
    {
        msg_box.setWindowIcon(QIcon(ICON));
    }

    // Close request confirmed.
    if (msg_box.exec() == QMessageBox::Ok)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool
Gui::show_config_window()
{
    // Create a configuration dialog.
    if (this->config_dialog != NULL)
    {
        delete this->config_dialog;
    }
    this->config_dialog = new Config_dialog(this->window);

    // Apply application settings if dialog is closed by OK.
    if (this->config_dialog->show() == QDialog::Accepted)
    {
        if (this->apply_application_settings() != true)
        {
            qCCritical(DS_APPSETTINGS) << "can not apply settings";
            return false;
        }
    }

    // Cleanup.
    delete this->config_dialog;
    this->config_dialog = NULL;

    return true;
}

void
Gui::set_fullscreen()
{
    if (this->window->isFullScreen() == false)
    {
        this->window->showFullScreen();
    }
    else
    {
        this->window->showNormal();
    }
}

void
Gui::show_help()
{
    if (this->help_groupbox->isHidden() == true)
    {
        this->help_groupbox->show();
    }
    else
    {
        this->help_groupbox->hide();
    }
}

void
Gui::set_focus_search_bar()
{
    this->file_search->setFocus();
    this->file_search->selectAll();
    this->search_from_begin = true;
}

void
Gui::press_enter_in_search_bar()
{
    this->search_from_begin = false;
    this->file_search_string(this->last_search_string);
}

void
Gui::press_esc_in_search_bar()
{
    this->file_browser->setFocus();
}


void
Gui::file_search_string(QString in_text)
{
    if (in_text != "")
    {
        // Store search text.
        this->last_search_string = in_text;

        // Search text in file browser (get a list of results).
        QModelIndexList items = this->file_system_model->search(in_text);

        if (this->search_from_begin == true)
        {
            // If we found file/dir name that match, return the first one.
            if (items.size() > 0)
            {
                // Select item in file browser.
                this->file_browser->setCurrentIndex(items[0]);
                this->file_browser_selected_index = 0;
            }
        }
        else
        {
            // If we found file/dir name that match, return the next one.
            if (items.size() > 0)
            {
                if ((int)this->file_browser_selected_index + 1 < items.size())
                {
                    // Select next item in file browser.
                    this->file_browser_selected_index++;
                }
                else
                {
                    // Wrap search, so select first item again.
                    this->file_browser_selected_index = 0;
                }
                this->file_browser->setCurrentIndex(items[this->file_browser_selected_index]);
            }

            this->search_from_begin = true;
        }
    }
}

void
Gui::analyze_audio_collection(bool is_all_files)
{
    // Analyzis not running, show a popup asking for a full refresh or only for new files.
    this->settings->set_audio_collection_full_refresh(is_all_files);

    // Show progress bar.
    this->progress_label->setText(tr("Analysing audio collection..."));
    this->progress_groupbox->show();

    // Compute data on file collection and store them to DB.
    this->file_system_model->concurrent_analyse_audio_collection();
}

void
Gui::update_refresh_progress_value(int in_value)
{
    this->progress_bar->setValue(in_value);

    if (this->file_system_model->concurrent_watcher_store->isRunning() == true)
    {
        // Refresh file browser during running file analyzis and storage.
        this->file_browser->update();
    }
}

void
Gui::on_finished_analyze_audio_collection()
{
    // Hide progress bar.
    this->progress_label->setText("");
    this->progress_groupbox->hide();

    // Refresh file browser.
    this->file_browser->setRootIndex(this->file_system_model->get_root_index());
    this->refresh_file_browser->setEnabled(true);
    this->refresh_file_browser->setChecked(false);
}

void
Gui::reject_refresh_audio_collection_dialog()
{
    this->refresh_file_browser->setEnabled(true);
    this->refresh_file_browser->setChecked(false);

    return;
}

void
Gui::close_refresh_audio_collection_dialog()
{
    if (this->refresh_audio_collection_dialog != NULL)
    {
        this->refresh_audio_collection_dialog->done(QDialog::Rejected);
    }

    return;
}

void
Gui::accept_refresh_audio_collection_dialog_all_files()
{
    // Analyze all files of audio collection.
    this->analyze_audio_collection(true);

    if (this->refresh_audio_collection_dialog != NULL)
    {
        this->refresh_audio_collection_dialog->done(QDialog::Accepted);
    }

    return;
}

void
Gui::accept_refresh_audio_collection_dialog_new_files()
{
    // Analyze all files of audio collection.
    this->analyze_audio_collection(false);

    if (this->refresh_audio_collection_dialog != NULL)
    {
        this->refresh_audio_collection_dialog->done(QDialog::Accepted);
    }

    return;
}

bool
Gui::show_refresh_audio_collection_dialog()
{
    // Show dialog only if there is no other analyzis process running.
    if (this->file_system_model->concurrent_watcher_store->isRunning() == false)
    {
        // Create the dialog object.
        if (this->refresh_audio_collection_dialog != NULL)
        {
            delete this->refresh_audio_collection_dialog;
        }
        this->refresh_audio_collection_dialog = new QDialog(this->window);

        // Set properties : title, icon.
        this->refresh_audio_collection_dialog->setWindowTitle(tr("Refresh audio collection"));
        if (this->nb_decks > 1)
        {
            this->refresh_audio_collection_dialog->setWindowIcon(QIcon(ICON_2));
        }
        else
        {
            this->refresh_audio_collection_dialog->setWindowIcon(QIcon(ICON));
        }

        // Main question.
        QLabel *main_question = new QLabel("<h3>" + tr("There are 2 possibilities to analyze the audio collection.") + "</h3>"
                                           + "<p>" + tr("Click the choice you would like") + "</p>",
                                           this->refresh_audio_collection_dialog);


        // Choice 1: => All files.
        QCommandLinkButton *choice_all_files_button = new QCommandLinkButton(tr("All files."),
                                                                             tr("Analyze full audio collection") + " (" + QString::number(this->file_system_model->get_nb_items()) + " " + tr("elements") + ")",
                                                                             this->refresh_audio_collection_dialog);
        QObject::connect(choice_all_files_button, &QCommandLinkButton::clicked,
                         [this](){this->accept_refresh_audio_collection_dialog_all_files();});

        // Choice 2: => New files.
        QCommandLinkButton *choice_new_files_button = new QCommandLinkButton(tr("New files."),
                                                                             tr("Analyze only files with missing data") + " (" + QString::number(this->file_system_model->get_nb_new_items()) + " " + tr("elements") + ")",
                                                                             this->refresh_audio_collection_dialog);
        QObject::connect(choice_new_files_button, &QCommandLinkButton::clicked,
                         [this](){this->accept_refresh_audio_collection_dialog_new_files();});

        // Close/cancel button.
        QDialogButtonBox *cancel_button = new QDialogButtonBox(QDialogButtonBox::Cancel);
        QObject::connect(cancel_button, &QDialogButtonBox::rejected,
                         [this](){this->close_refresh_audio_collection_dialog();});
        QObject::connect(refresh_audio_collection_dialog, &QDialog::rejected,
                         [this](){this->reject_refresh_audio_collection_dialog();});

        // Full dialog layout.
        QVBoxLayout *layout = new QVBoxLayout(this->refresh_audio_collection_dialog);
        layout->addWidget(main_question);
        layout->addWidget(choice_all_files_button);
        layout->addWidget(choice_new_files_button);
        layout->addWidget(cancel_button);

        // Put layout in dialog.
        this->refresh_audio_collection_dialog->setLayout(layout);
        layout->setSizeConstraint(QLayout::SetFixedSize);

        // Show dialog.
        this->refresh_audio_collection_dialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        this->refresh_audio_collection_dialog->adjustSize();
        this->refresh_audio_collection_dialog->exec();

        // Cleanup.
        delete this->refresh_audio_collection_dialog;
        this->refresh_audio_collection_dialog = NULL;
    }
    else
    {
        // Analyzis already running. Cancel it.
        this->file_system_model->stop_concurrent_analyse_audio_collection();
    }

    return true;
}

void
Gui::show_hide_samplers()
{
    if (this->sampler1_widget->isVisible() == true)
    {
        // Hide samplers.
        this->hide_samplers();
    }
    else
    {
        // Show samplers.
        this->show_samplers();
    }
}

void
Gui::hide_samplers()
{
    this->show_hide_samplers_button->setChecked(false);
    this->settings->set_samplers_visible(false);
    this->sampler1_widget->setVisible(false);
    this->sampler1_gbox->setMaximumHeight(20);
    if (this->nb_decks > 1)
    {
        this->sampler2_widget->setVisible(false);
        this->sampler2_gbox->setMaximumHeight(20);
    }
}

void
Gui::show_samplers()
{
    this->show_hide_samplers_button->setChecked(true);
    this->settings->set_samplers_visible(true);
    this->sampler1_gbox->setMaximumHeight(999);
    this->sampler1_widget->setVisible(true);
    if (this->nb_decks > 1)
    {
        this->sampler2_gbox->setMaximumHeight(999);
        this->sampler2_widget->setVisible(true);
    }
}

void
Gui::done_about_window()
{
    if (this->about_dialog != NULL)
    {
        this->about_dialog->done(QDialog::Accepted);
    }

    return;
}

bool
Gui::show_about_window()
{
    // Create about window.
    if (this->about_dialog != NULL)
    {
        delete this->about_dialog;
    }
    this->about_dialog = new QDialog(this->window);

    // Set properties : title, icon.
    this->about_dialog->setWindowTitle(tr("About DigitalScratch"));
    if (this->nb_decks > 1)
    {
        this->about_dialog->setWindowIcon(QIcon(ICON_2));
    }
    else
    {
        this->about_dialog->setWindowIcon(QIcon(ICON));
    }


    //
    // Set content (logo, name-version, description, credit, license, libraries).
    //
    QLabel *logo = new QLabel();
    logo->setPixmap(QPixmap(LOGO));
    logo->setAlignment(Qt::AlignHCenter);

    QString version = QString("<h1>DigitalScratch ") + QString(STR(VERSION)) + QString("</h1>");
    QLabel *name = new QLabel(tr(version.toUtf8()));
    name->setAlignment(Qt::AlignHCenter);
    name->setTextFormat(Qt::RichText);

    QLabel *description = new QLabel(tr("A vinyl emulation software."));
    description->setAlignment(Qt::AlignHCenter);

    QLabel *web_site = new QLabel("<a style=\"color: orange\" href=\"http://www.digital-scratch.org\">http://www.digital-scratch.org</a>");
    web_site->setAlignment(Qt::AlignHCenter);
    web_site->setTextFormat(Qt::RichText);
    web_site->setTextInteractionFlags(Qt::TextBrowserInteraction);
    web_site->setOpenExternalLinks(true);

    QLabel *credit = new QLabel(tr("Copyright (C) 2003-2014 Julien Rosener"));
    credit->setAlignment(Qt::AlignHCenter);

    QLabel *license = new QLabel(tr("This program is free software; you can redistribute it and/or modify <br/>\
                                     it under the terms of the GNU General Public License as published by <br/>\
                                     the Free Software Foundation; either version 3 of the License, or <br/>\
                                     (at your option) any later version.<br/><br/>"));
    license->setTextFormat(Qt::RichText);
    license->setAlignment(Qt::AlignHCenter);

    QLabel *built = new QLabel("<b>" + tr("Built with:") + "</b>");
    built->setTextFormat(Qt::RichText);
    QLabel *qt_version                = new QLabel((QString("- Qt v") + QString(qVersion())).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://qt-project.org\">http://qt-project.org</a>");
    QLabel *libdigitalscratch_version = new QLabel((QString("- libdigitalscratch v") + QString(dscratch_get_version())).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://www.digital-scratch.org\">http://www.digital-scratch.org</a>");
    QLabel *libavcodec_version        = new QLabel((QString("- libavcodec v") + QString::number(LIBAVCODEC_VERSION_MAJOR) + QString(".") + QString::number(LIBAVCODEC_VERSION_MINOR) + QString(".") + QString::number(LIBAVCODEC_VERSION_MICRO)).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://www.libav.org\">http://www.libav.org</a>");
    QLabel *libavformat_version       = new QLabel((QString("- libavformat v") + QString::number(LIBAVFORMAT_VERSION_MAJOR) + QString(".") + QString::number(LIBAVFORMAT_VERSION_MINOR) + QString(".") + QString::number(LIBAVFORMAT_VERSION_MICRO)).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://www.libav.org\">http://www.libav.org</a>");
    QLabel *libavutil_version         = new QLabel((QString("- libavutil v") + QString::number(LIBAVUTIL_VERSION_MAJOR) + QString(".") + QString::number(LIBAVUTIL_VERSION_MINOR) + QString(".") + QString::number(LIBAVUTIL_VERSION_MICRO)).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://www.libav.org\">http://www.libav.org</a>");
    QLabel *libsamplerate_version     = new QLabel((QString("- ") + QString(src_get_version())).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://www.mega-nerd.com/SRC/\">http://www.mega-nerd.com/SRC/</a>");
    QLabel *libjack_version           = new QLabel((QString("- libjack v") + QString(jack_get_version_string())).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://jackaudio.org\">http://jackaudio.org</a>");
    QLabel *libkeyfinder_version      = new QLabel((QString("- libkeyfinder v") + QString(kfinder_get_version())).toUtf8()
                                                   + ", <a style=\"color: grey\" href=\"http://www.ibrahimshaath.co.uk/keyfinder/\">http://www.ibrahimshaath.co.uk/keyfinder/</a>");

    qt_version->setTextFormat(Qt::RichText);
    qt_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    qt_version->setOpenExternalLinks(true);

    libdigitalscratch_version->setTextFormat(Qt::RichText);
    libdigitalscratch_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    libdigitalscratch_version->setOpenExternalLinks(true);

    libavcodec_version->setTextFormat(Qt::RichText);
    libavcodec_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    libavcodec_version->setOpenExternalLinks(true);

    libavformat_version->setTextFormat(Qt::RichText);
    libavformat_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    libavformat_version->setOpenExternalLinks(true);

    libavutil_version->setTextFormat(Qt::RichText);
    libavutil_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    libavutil_version->setOpenExternalLinks(true);

    libsamplerate_version->setTextFormat(Qt::RichText);
    libsamplerate_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    libsamplerate_version->setOpenExternalLinks(true);

    libjack_version->setTextFormat(Qt::RichText);
    libjack_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    libjack_version->setOpenExternalLinks(true);

    libkeyfinder_version->setTextFormat(Qt::RichText);
    libkeyfinder_version->setTextInteractionFlags(Qt::TextBrowserInteraction);
    libkeyfinder_version->setOpenExternalLinks(true);

    QLabel *credits = new QLabel("<br/><b>" + tr("Credits:") + "</b>");
    credits->setTextFormat(Qt::RichText);
    QLabel *icons = new QLabel("- Devine icons: <a style=\"color: grey\" href=\"http://ipapun.deviantart.com\">http://ipapun.deviantart.com</a>");
    icons->setTextFormat(Qt::RichText);
    icons->setTextInteractionFlags(Qt::TextBrowserInteraction);
    icons->setOpenExternalLinks(true);

    QLabel *help = new QLabel("<br/><b>" + tr("Help:") + "</b>");
    help->setTextFormat(Qt::RichText);
    QLabel *wiki = new QLabel("- Online help: <a style=\"color: grey\" href=\"https://github.com/jrosener/digitalscratch/wiki\">https://github.com/jrosener/digitalscratch/wiki</a>");
    wiki->setTextFormat(Qt::RichText);
    wiki->setTextInteractionFlags(Qt::TextBrowserInteraction);
    wiki->setOpenExternalLinks(true);

    // Close button.
    QDialogButtonBox *button = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(button, &QDialogButtonBox::rejected,
                     [this](){this->done_about_window();});

    // Full window layout.
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(logo, Qt::AlignHCenter);
    layout->addWidget(name, Qt::AlignHCenter);
    layout->addWidget(description, Qt::AlignHCenter);
    layout->addWidget(web_site, Qt::AlignHCenter);
    layout->addWidget(credit);
    layout->addWidget(license);
    layout->addWidget(built);
    layout->addWidget(qt_version);
    layout->addWidget(libdigitalscratch_version);
    layout->addWidget(libavcodec_version);
    layout->addWidget(libavformat_version);
    layout->addWidget(libavutil_version);
    layout->addWidget(libsamplerate_version);
    layout->addWidget(libjack_version);
    layout->addWidget(libkeyfinder_version);
    layout->addWidget(credits);
    layout->addWidget(icons);
    layout->addWidget(help);
    layout->addWidget(wiki);
    layout->addWidget(button);

    // Put layout in dialog.
    this->about_dialog->setLayout(layout);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // Show dialog.
    this->about_dialog->exec();

    // Cleanup.
    delete this->about_dialog;
    this->about_dialog = NULL;

    return true;
}

void
Gui::done_error_window()
{
    if (this->error_dialog != NULL)
    {
        this->error_dialog->done(QDialog::Accepted);
    }

    return;
}

bool
Gui::show_error_window(QString in_error_message)
{
    // Prepare error window.
    QMessageBox msg_box;
    msg_box.setWindowTitle("DigitalScratch");
    msg_box.setText("<h2>" + tr("Error") + "</h2>"
                    + "<br/>" + in_error_message
                    + "<br/><br/>" + "Please fix this issue and restart DigitalScratch.");
    msg_box.setStandardButtons(QMessageBox::Close);
    msg_box.setIcon(QMessageBox::Critical);
    msg_box.setStyleSheet(Utils::get_current_stylesheet_css());
    if (this->nb_decks > 1)
    {
        msg_box.setWindowIcon(QIcon(ICON_2));
    }
    else
    {
        msg_box.setWindowIcon(QIcon(ICON));
    }

    // Show error dialog.
    msg_box.exec();

    return true;
}

bool
Gui::create_main_window()
{
    // Init main window.
    this->window             = new QWidget();
    this->is_window_rendered = false;
    this->window_style       = GUI_STYLE_DEFAULT;

    // Init areas (header buttons, decks, samplers, file browser, bottom help, bottom status bar).
    this->init_header_buttons();
    this->init_decks_area();
    this->init_samplers_area();
    this->init_file_browser_area();
    this->init_bottom_help();
    this->init_bottom_status();

    // Bind buttons/shortcuts to events.
    this->connect_header_buttons();
    this->connect_decks_area();
    this->connect_samplers_area();
    this->connect_decks_and_samplers_selection();
    this->connect_file_browser_area();

    // Create main window.
    this->window->setWindowTitle(tr("DigitalScratch") + " " + QString(STR(VERSION)));
    this->window->setMinimumSize(800, 480);
    if (this->nb_decks > 1)
    {
        this->window->setWindowIcon(QIcon(ICON_2));
    }
    else
    {
        this->window->setWindowIcon(QIcon(ICON));
    }

    // Create main vertical layout.
    QVBoxLayout *main_layout = new QVBoxLayout;

    // Set main layout in main window.
    this->window->setLayout(main_layout);

    // Put every components in main layout.
    main_layout->addLayout(this->header_layout,   5);
    main_layout->addLayout(this->decks_layout,    30);
    main_layout->addLayout(this->samplers_layout, 5);
    main_layout->addLayout(this->file_layout,     65);
    main_layout->addLayout(this->bottom_layout,   0);
    main_layout->addLayout(this->status_layout,   0);

    // Display main window.
    this->window->show();

    // Set window position/size from last run.
    this->window->move(this->settings->get_main_window_position());
    this->window->resize(this->settings->get_main_window_size());

    // Open error window.
    QObject::connect(this->sound_card.data(), &Sound_driver_access_rules::error_msg,
                     [this](QString in_error_message){this->show_error_window(in_error_message);});
    return true;
}

void
Gui::init_header_buttons()
{
    // Create configuration button.
    this->config_button = new QPushButton("   " + tr("&Settings"));
    this->config_button->setToolTip(tr("Change application settings..."));
    this->config_button->setObjectName("Configuration_button");
    this->config_button->setFocusPolicy(Qt::NoFocus);

    // Create button to set full screen.
    this->fullscreen_button = new QPushButton("   " + tr("&Full-screen"));
    this->fullscreen_button->setToolTip("<p>" + tr("Toggle fullscreen mode") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_FULLSCREEN) + "</em>");
    this->fullscreen_button->setObjectName("Fullscreen_button");
    this->fullscreen_button->setFocusPolicy(Qt::NoFocus);

    // Create Stop capture button.
    this->stop_capture_button = new QPushButton(tr("S&TOP"));
    this->stop_capture_button->setToolTip("<p>" + tr("Stop turntable motion detection") + "</p>");
    this->stop_capture_button->setObjectName("Capture_buttons");
    this->stop_capture_button->setFocusPolicy(Qt::NoFocus);
    this->stop_capture_button->setCheckable(true);
    this->stop_capture_button->setChecked(true);

    // Create DigitalScratch logo.
    this->logo = new QPushButton();
    this->logo->setToolTip(tr("About DigitalScratch..."));
    this->logo->setObjectName("Logo");
    this->logo->setIcon(QIcon(LOGO));
    this->logo->setIconSize(QSize(112, 35));
    this->logo->setMaximumWidth(112);
    this->logo->setMaximumHeight(35);
    this->logo->setFlat(true);
    this->logo->setFocusPolicy(Qt::NoFocus);

    // Create Start capture button.
    this->start_capture_button = new QPushButton(tr("ST&ART"));
    this->start_capture_button->setToolTip("<p>" + tr("Start turntable motion detection") + "</p>");
    this->start_capture_button->setObjectName("Capture_buttons");
    this->start_capture_button->setFocusPolicy(Qt::NoFocus);
    this->start_capture_button->setCheckable(true);
    this->start_capture_button->setChecked(false);

    // Create help button.
    this->help_button = new QPushButton("   " + tr("&Help"));
    this->help_button->setToolTip("<p>" + tr("Show/hide keyboard shortcuts") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_HELP) + "</em>");
    this->help_button->setObjectName("Help_button");
    this->help_button->setFocusPolicy(Qt::NoFocus);

    // Create quit button.
    this->quit_button = new QPushButton("   " + tr("&Exit"));
    this->quit_button->setToolTip(tr("Exit DigitalScratch"));
    this->quit_button->setObjectName("Quit_button");
    this->quit_button->setFocusPolicy(Qt::NoFocus);

    // Create top horizontal layout.
    this->header_layout = new QHBoxLayout();

    // Put configuration button and logo in configuration layout.
    this->header_layout->addWidget(this->config_button,        1,   Qt::AlignLeft);
    this->header_layout->addWidget(this->fullscreen_button,    1,   Qt::AlignLeft);
    this->header_layout->addWidget(this->stop_capture_button,  100, Qt::AlignRight);
    this->header_layout->addWidget(this->logo,                 1,   Qt::AlignCenter);
    this->header_layout->addWidget(this->start_capture_button, 100, Qt::AlignLeft);
    this->header_layout->addWidget(this->help_button,          1,   Qt::AlignRight);
    this->header_layout->addWidget(this->quit_button,          1,   Qt::AlignRight);
}

void
Gui::clean_header_buttons()
{
    delete this->config_button;
    delete this->fullscreen_button;
    delete this->stop_capture_button;
    delete this->logo;
    delete this->start_capture_button;
    delete this->help_button;
    delete this->quit_button;
    delete this->header_layout;
}

void
Gui::connect_header_buttons()
{   
    // Open configuration window.
    QObject::connect(this->config_button, &QPushButton::clicked, [this](){this->show_config_window();});

    // Set full screen.
    QObject::connect(this->fullscreen_button, &QPushButton::clicked, [this](){this->set_fullscreen();});
    this->shortcut_fullscreen = new QShortcut(this->window);
    QObject::connect(this->shortcut_fullscreen, &QShortcut::activated, [this](){this->set_fullscreen();});

    // Stop capture.
    QObject::connect(this->stop_capture_button, &QPushButton::clicked, [this](){this->stop_control_and_playback();});

    // Open about window.
    QObject::connect(this->logo, &QPushButton::clicked, [this](){show_about_window();});

    // Start capture.
    QObject::connect(this->start_capture_button, &QPushButton::clicked, [this](){this->start_control_and_playback();});

    // Help button.
    QObject::connect(this->help_button, &QPushButton::clicked, [this](){this->show_help();});
    this->shortcut_help = new QShortcut(this->window);
    QObject::connect(this->shortcut_help, &QShortcut::activated, [this](){this->show_help();});

    // Quit application.
    QObject::connect(this->quit_button, &QPushButton::clicked, [this](){this->can_close();});
}

void
Gui::init_decks_area()
{
    // Init common elements for deck 1 and 2.
    this->decks_remaining_time = new Remaining_time* [2];

    // Init decks.
    this->init_deck1_area();
    this->init_deck2_area();

    // Create horizontal and vertical layout for each deck.
    QHBoxLayout *deck1_general_layout = new QHBoxLayout();
    QHBoxLayout *deck2_general_layout = new QHBoxLayout();
    QVBoxLayout *deck1_layout = new QVBoxLayout();
    QVBoxLayout *deck2_layout = new QVBoxLayout();

    // Put track name, position and timecode info in decks layout.
    deck1_layout->addWidget(this->deck1_track_name,            5);
    deck1_layout->addLayout(this->deck1_remaining_time_layout, 5);
    deck1_layout->addWidget(this->deck1_waveform,              85);
    deck1_layout->addLayout(this->deck1_buttons_layout,        5);
    deck1_general_layout->addLayout(deck1_layout,              90);

    deck2_layout->addWidget(this->deck2_track_name,            5);
    deck2_layout->addLayout(this->deck2_remaining_time_layout, 5);
    deck2_layout->addWidget(this->deck2_waveform,              85);
    deck2_layout->addLayout(this->deck2_buttons_layout,        5);
    deck2_general_layout->addLayout(deck2_layout,              90);

    // Create deck group boxes.
    this->deck1_gbox = new PlaybackQGroupBox(tr("Deck 1"));
    this->deck1_gbox->setObjectName("DeckGBox");
    this->deck2_gbox = new PlaybackQGroupBox(tr("Deck 2"));
    this->deck2_gbox->setObjectName("DeckGBox");    

    // Put horizontal layouts in group boxes.
    this->deck1_gbox->setLayout(deck1_general_layout);
    this->deck2_gbox->setLayout(deck2_general_layout);

    // Create horizontal layout for 2 decks.
    this->decks_layout = new QHBoxLayout;

    // Put decks in layout.
    this->decks_layout->addWidget(this->deck1_gbox);
    if (this->nb_decks > 1)
    {
        this->decks_layout->addWidget(this->deck2_gbox);
    }
}

void
Gui::init_deck1_area()
{
    // Create track name, key and waveform.
    this->deck1_track_name = new QLabel(tr("T r a c k  # 1"));
    this->deck1_key        = new QLabel();
    this->deck1_waveform   = new Waveform(this->ats[0], this->window);
    this->deck1_track_name->setObjectName("TrackName");
    this->deck1_key->setObjectName("KeyValue");
    this->set_deck1_key("");
    this->deck1_waveform->setObjectName("Waveform");

    // Create remaining time.
    this->deck1_remaining_time_layout = new QHBoxLayout;
    this->decks_remaining_time[0] = new Remaining_time();
    this->deck1_remaining_time_layout->addWidget(this->decks_remaining_time[0]->minus, 1, Qt::AlignBottom);
    this->deck1_remaining_time_layout->addWidget(this->decks_remaining_time[0]->min,   1, Qt::AlignBottom);
    this->deck1_remaining_time_layout->addWidget(this->decks_remaining_time[0]->sep1,  1, Qt::AlignBottom);
    this->deck1_remaining_time_layout->addWidget(this->decks_remaining_time[0]->sec,   1, Qt::AlignBottom);
    this->deck1_remaining_time_layout->addWidget(this->decks_remaining_time[0]->sep2,  1, Qt::AlignBottom);
    this->deck1_remaining_time_layout->addWidget(this->decks_remaining_time[0]->msec,  1, Qt::AlignBottom);
    this->deck1_remaining_time_layout->addStretch(100);
    this->deck1_remaining_time_layout->addWidget(this->deck1_key,                      1, Qt::AlignRight);

    // Create buttons area.
    this->deck1_buttons_layout = new QHBoxLayout();

    // Speed management.
    QVBoxLayout *deck1_timecode_speed_layout = new QVBoxLayout();
    this->deck1_timecode_manual_button = new QPushButton(tr("TIMECODE"));
    this->deck1_timecode_manual_button->setToolTip("<p>" + tr("Switch speed mode TIMECODE/MANUAL.") + "</p>");
    this->deck1_timecode_manual_button->setObjectName("Timecode_toggle");
    this->deck1_timecode_manual_button->setFocusPolicy(Qt::NoFocus);
    this->deck1_timecode_manual_button->setCheckable(true);
    this->deck1_timecode_manual_button->setChecked(true);
    deck1_timecode_speed_layout->addWidget(this->deck1_timecode_manual_button);
    this->deck1_speed = new QLabel(tr("+000.0%"));
    this->deck1_speed->setObjectName("Speed_value");
    deck1_timecode_speed_layout->addWidget(this->deck1_speed);
    this->deck1_buttons_layout->addLayout(deck1_timecode_speed_layout);
    QGridLayout *deck1_speed_layout = new QGridLayout();
    this->speed_up_on_deck1_button = new SpeedQPushButton("+");
    this->speed_up_on_deck1_button->setToolTip("<p>" + tr("+0.1% speed up") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->speed_up_on_deck1_button->setObjectName("Speed_button");
    this->speed_up_on_deck1_button->setFocusPolicy(Qt::NoFocus);
    this->speed_up_on_deck1_button->setFixedSize(15, 15);
    deck1_speed_layout->addWidget(speed_up_on_deck1_button, 0, 1);
    this->speed_down_on_deck1_button = new SpeedQPushButton("-");
    this->speed_down_on_deck1_button->setToolTip("<p>" + tr("-0.1% slow down") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->speed_down_on_deck1_button->setObjectName("Speed_button");
    this->speed_down_on_deck1_button->setFocusPolicy(Qt::NoFocus);
    this->speed_down_on_deck1_button->setFixedSize(15, 15);
    deck1_speed_layout->addWidget(speed_down_on_deck1_button, 1, 1);
    this->accel_up_on_deck1_button = new SpeedQPushButton("↷");
    this->accel_up_on_deck1_button->setToolTip("<p>" + tr("Temporarily speed up") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->accel_up_on_deck1_button->setObjectName("Speed_button");
    this->accel_up_on_deck1_button->setFocusPolicy(Qt::NoFocus);
    this->accel_up_on_deck1_button->setFixedSize(15, 15);
    deck1_speed_layout->addWidget(accel_up_on_deck1_button, 0, 2);
    this->accel_down_on_deck1_button = new SpeedQPushButton("↶");
    this->accel_down_on_deck1_button->setToolTip("<p>" + tr("Temporarily slow down") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->accel_down_on_deck1_button->setObjectName("Speed_button");
    this->accel_down_on_deck1_button->setFocusPolicy(Qt::NoFocus);
    this->accel_down_on_deck1_button->setFixedSize(15, 15);
    deck1_speed_layout->addWidget(accel_down_on_deck1_button, 1, 2);
    this->deck1_buttons_layout->addLayout(deck1_speed_layout);
    this->deck1_buttons_layout->addStretch(1000);

    // Select speed control mode.
    this->switch_speed_mode(true, 0);

    // Restart button.
    this->restart_on_deck1_button = new QPushButton();
    this->restart_on_deck1_button->setObjectName("Restart_button");
    this->restart_on_deck1_button->setToolTip("<p>" + tr("Jump to start") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->restart_on_deck1_button->setFixedSize(15, 15);
    this->restart_on_deck1_button->setFocusPolicy(Qt::NoFocus);
    this->restart_on_deck1_button->setCheckable(true);
    deck1_buttons_layout->addWidget(this->restart_on_deck1_button, 1, Qt::AlignLeft | Qt::AlignTop);

    // Cue point management.
    this->cue_set_on_deck1_buttons  = new QPushButton* [MAX_NB_CUE_POINTS];
    this->cue_play_on_deck1_buttons = new QPushButton* [MAX_NB_CUE_POINTS];
    this->cue_del_on_deck1_buttons  = new QPushButton* [MAX_NB_CUE_POINTS];
    this->cue_point_deck1_labels    = new QLabel* [MAX_NB_CUE_POINTS];
    for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
    {
        this->cue_set_on_deck1_buttons[i] = new QPushButton();
        this->cue_set_on_deck1_buttons[i]->setObjectName("Cue_set_button" + QString::number(i));
        this->cue_set_on_deck1_buttons[i]->setToolTip("<p>" + tr("Set cue point") + " " + QString::number(i+1) + "</p><em>" + this->settings->get_keyboard_shortcut(KB_SET_CUE_POINTS_ON_DECK[i]) + "</em>");
        this->cue_set_on_deck1_buttons[i]->setFixedSize(15, 15);
        this->cue_set_on_deck1_buttons[i]->setFocusPolicy(Qt::NoFocus);
        this->cue_set_on_deck1_buttons[i]->setCheckable(true);

        this->cue_play_on_deck1_buttons[i] = new QPushButton();
        this->cue_play_on_deck1_buttons[i]->setObjectName("Cue_play_button" + QString::number(i));
        this->cue_play_on_deck1_buttons[i]->setToolTip("<p>" + tr("Play from cue point") + " " + QString::number(i+1) + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_CUE_POINTS_ON_DECK[i]) + "</em>");
        this->cue_play_on_deck1_buttons[i]->setFixedSize(15, 15);
        this->cue_play_on_deck1_buttons[i]->setFocusPolicy(Qt::NoFocus);
        this->cue_play_on_deck1_buttons[i]->setCheckable(true);

        this->cue_del_on_deck1_buttons[i] = new QPushButton();
        this->cue_del_on_deck1_buttons[i]->setObjectName("Cue_del_button" + QString::number(i));
        this->cue_del_on_deck1_buttons[i]->setToolTip("<p>" + tr("Delete cue point") + " " + QString::number(i+1));
        this->cue_del_on_deck1_buttons[i]->setFixedSize(15, 15);
        this->cue_del_on_deck1_buttons[i]->setFocusPolicy(Qt::NoFocus);
        this->cue_del_on_deck1_buttons[i]->setCheckable(true);

        this->cue_point_deck1_labels[i] = new QLabel("__:__:___");
        this->cue_point_deck1_labels[i]->setObjectName("Cue_point_label");
        this->cue_point_deck1_labels[i]->setAlignment(Qt::AlignCenter);

        QHBoxLayout *deck1_cue_buttons_layout = new QHBoxLayout();
        deck1_cue_buttons_layout->addWidget(this->cue_set_on_deck1_buttons[i],  1, Qt::AlignRight);
        deck1_cue_buttons_layout->addWidget(this->cue_play_on_deck1_buttons[i], 1, Qt::AlignRight);
        deck1_cue_buttons_layout->addWidget(this->cue_del_on_deck1_buttons[i],  1, Qt::AlignRight);

        QVBoxLayout *deck1_cue_points_layout = new QVBoxLayout();
        deck1_cue_points_layout->addLayout(deck1_cue_buttons_layout);
        deck1_cue_points_layout->addWidget(this->cue_point_deck1_labels[i], Qt::AlignCenter);

        this->deck1_buttons_layout->addStretch(5);
        this->deck1_buttons_layout->addLayout(deck1_cue_points_layout, 1);
    }
}

void
Gui::init_deck2_area()
{
    // Create track name, key and waveform.
    this->deck2_track_name = new QLabel(tr("T r a c k  # 2"));
    this->deck2_key        = new QLabel();
    this->deck2_waveform   = new Waveform(this->ats[1], this->window);
    this->deck2_track_name->setObjectName("TrackName");
    this->deck2_key->setObjectName("KeyValue");
    this->set_deck2_key("");
    this->deck2_waveform->setObjectName("Waveform");

    // Create remaining time.
    this->deck2_remaining_time_layout = new QHBoxLayout;
    this->decks_remaining_time[1] = new Remaining_time();
    this->deck2_remaining_time_layout->addWidget(this->decks_remaining_time[1]->minus, 1, Qt::AlignBottom);
    this->deck2_remaining_time_layout->addWidget(this->decks_remaining_time[1]->min,   1, Qt::AlignBottom);
    this->deck2_remaining_time_layout->addWidget(this->decks_remaining_time[1]->sep1,  1, Qt::AlignBottom);
    this->deck2_remaining_time_layout->addWidget(this->decks_remaining_time[1]->sec,   1, Qt::AlignBottom);
    this->deck2_remaining_time_layout->addWidget(this->decks_remaining_time[1]->sep2,  1, Qt::AlignBottom);
    this->deck2_remaining_time_layout->addWidget(this->decks_remaining_time[1]->msec,  1, Qt::AlignBottom);
    this->deck2_remaining_time_layout->addStretch(100);
    this->deck2_remaining_time_layout->addWidget(this->deck2_key,                      1, Qt::AlignRight);

    // Create buttons area.
    this->deck2_buttons_layout = new QHBoxLayout();

    // Speed management.
    this->deck2_speed = new QLabel(tr("+000.0%"));
    this->deck2_speed->setObjectName("Speed_value");
    this->deck2_buttons_layout->addWidget(this->deck2_speed);
    QGridLayout *deck2_speed_layout = new QGridLayout();
    this->speed_up_on_deck2_button = new SpeedQPushButton("+");
    this->speed_up_on_deck2_button->setToolTip("<p>" + tr("+0.1% speed up") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->speed_up_on_deck2_button->setObjectName("Speed_button");
    this->speed_up_on_deck2_button->setFocusPolicy(Qt::NoFocus);
    this->speed_up_on_deck2_button->setFixedSize(15, 15);
    deck2_speed_layout->addWidget(speed_up_on_deck2_button, 0, 1);
    this->speed_down_on_deck2_button = new SpeedQPushButton("-");
    this->speed_down_on_deck2_button->setToolTip("<p>" + tr("-0.1% slow down") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->speed_down_on_deck2_button->setObjectName("Speed_button");
    this->speed_down_on_deck2_button->setFocusPolicy(Qt::NoFocus);
    this->speed_down_on_deck2_button->setFixedSize(15, 15);
    deck2_speed_layout->addWidget(speed_down_on_deck2_button, 1, 1);
    this->accel_up_on_deck2_button = new SpeedQPushButton("↷");
    this->accel_up_on_deck2_button->setToolTip("<p>" + tr("Temporarily speed up") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->accel_up_on_deck2_button->setObjectName("Speed_button");
    this->accel_up_on_deck2_button->setFocusPolicy(Qt::NoFocus);
    this->accel_up_on_deck2_button->setFixedSize(15, 15);
    deck2_speed_layout->addWidget(accel_up_on_deck2_button, 0, 2);
    this->accel_down_on_deck2_button = new SpeedQPushButton("↶");
    this->accel_down_on_deck2_button->setToolTip("<p>" + tr("Temporarily slow down") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->accel_down_on_deck2_button->setObjectName("Speed_button");
    this->accel_down_on_deck2_button->setFocusPolicy(Qt::NoFocus);
    this->accel_down_on_deck2_button->setFixedSize(15, 15);
    deck2_speed_layout->addWidget(accel_down_on_deck2_button, 1, 2);
    this->deck2_buttons_layout->addLayout(deck2_speed_layout);
    this->deck2_buttons_layout->addStretch(1000);

    // Restart button.
    this->restart_on_deck2_button = new QPushButton();
    this->restart_on_deck2_button->setObjectName("Restart_button");
    this->restart_on_deck2_button->setToolTip("<p>" + tr("Jump to start") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK) + "</em>");
    this->restart_on_deck2_button->setFixedSize(15, 15);
    this->restart_on_deck2_button->setFocusPolicy(Qt::NoFocus);
    this->restart_on_deck2_button->setCheckable(true);
    deck2_buttons_layout->addWidget(this->restart_on_deck2_button, 1, Qt::AlignLeft | Qt::AlignTop);

    // Cue point management.
    this->cue_set_on_deck2_buttons  = new QPushButton* [MAX_NB_CUE_POINTS];
    this->cue_play_on_deck2_buttons = new QPushButton* [MAX_NB_CUE_POINTS];
    this->cue_del_on_deck2_buttons  = new QPushButton* [MAX_NB_CUE_POINTS];
    this->cue_point_deck2_labels    = new QLabel* [MAX_NB_CUE_POINTS];
    for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
    {
        this->cue_set_on_deck2_buttons[i] = new QPushButton();
        this->cue_set_on_deck2_buttons[i]->setObjectName("Cue_set_button" + QString::number(i));
        this->cue_set_on_deck2_buttons[i]->setToolTip("<p>" + tr("Set cue point") + " " + QString::number(i+1) + "</p><em>" + this->settings->get_keyboard_shortcut(KB_SET_CUE_POINTS_ON_DECK[i]) + "</em>");
        this->cue_set_on_deck2_buttons[i]->setFixedSize(15, 15);
        this->cue_set_on_deck2_buttons[i]->setFocusPolicy(Qt::NoFocus);
        this->cue_set_on_deck2_buttons[i]->setCheckable(true);

        this->cue_play_on_deck2_buttons[i] = new QPushButton();
        this->cue_play_on_deck2_buttons[i]->setObjectName("Cue_play_button" + QString::number(i));
        this->cue_play_on_deck2_buttons[i]->setToolTip("<p>" + tr("Play from cue point") + " " + QString::number(i+1) + "</p><em>" + this->settings->get_keyboard_shortcut(KB_PLAY_CUE_POINTS_ON_DECK[i]) + "</em>");
        this->cue_play_on_deck2_buttons[i]->setFixedSize(15, 15);
        this->cue_play_on_deck2_buttons[i]->setFocusPolicy(Qt::NoFocus);
        this->cue_play_on_deck2_buttons[i]->setCheckable(true);

        this->cue_del_on_deck2_buttons[i] = new QPushButton();
        this->cue_del_on_deck2_buttons[i]->setObjectName("Cue_del_button" + QString::number(i));
        this->cue_del_on_deck2_buttons[i]->setToolTip("<p>" + tr("Delete cue point") + " " + QString::number(i+1));
        this->cue_del_on_deck2_buttons[i]->setFixedSize(15, 15);
        this->cue_del_on_deck2_buttons[i]->setFocusPolicy(Qt::NoFocus);
        this->cue_del_on_deck2_buttons[i]->setCheckable(true);

        this->cue_point_deck2_labels[i] = new QLabel("__:__:___");
        this->cue_point_deck2_labels[i]->setObjectName("Cue_point_label");
        this->cue_point_deck2_labels[i]->setAlignment(Qt::AlignCenter);

        QHBoxLayout *deck2_cue_buttons_layout = new QHBoxLayout();
        deck2_cue_buttons_layout->addWidget(this->cue_set_on_deck2_buttons[i],  1, Qt::AlignRight);
        deck2_cue_buttons_layout->addWidget(this->cue_play_on_deck2_buttons[i], 1, Qt::AlignRight);
        deck2_cue_buttons_layout->addWidget(this->cue_del_on_deck2_buttons[i],  1, Qt::AlignRight);

        QVBoxLayout *deck2_cue_points_layout = new QVBoxLayout();
        deck2_cue_points_layout->addLayout(deck2_cue_buttons_layout);
        deck2_cue_points_layout->addWidget(this->cue_point_deck2_labels[i], Qt::AlignCenter);

        this->deck2_buttons_layout->addStretch(5);
        this->deck2_buttons_layout->addLayout(deck2_cue_points_layout, 1);
    }
}

void
Gui::clean_decks_area()
{
    for (int i = 0; i < nb_decks; i++)
    {
        delete this->decks_remaining_time[i];
    }
    delete [] this->decks_remaining_time;

    delete this->deck1_track_name;
    delete this->deck1_key;
    delete this->deck1_waveform;
    delete this->deck1_remaining_time_layout;
    delete this->deck1_buttons_layout;
    delete this->deck1_speed;
    delete this->deck1_timecode_manual_button;
    delete this->speed_up_on_deck1_button;
    delete this->speed_down_on_deck1_button;
    delete this->accel_up_on_deck1_button;
    delete this->accel_down_on_deck1_button;
    delete this->restart_on_deck1_button;
    delete [] this->cue_set_on_deck1_buttons;
    delete [] this->cue_play_on_deck1_buttons;
    delete [] this->cue_del_on_deck1_buttons;
    delete [] this->cue_point_deck1_labels;

    delete this->deck2_track_name;
    delete this->deck2_key;
    delete this->deck2_waveform;
    delete this->deck2_remaining_time_layout;
    delete this->deck2_buttons_layout;
    delete this->deck2_speed;
    delete this->speed_up_on_deck2_button;
    delete this->speed_down_on_deck2_button;
    delete this->accel_up_on_deck2_button;
    delete this->accel_down_on_deck2_button;
    delete this->restart_on_deck2_button;
    delete [] this->cue_set_on_deck2_buttons;
    delete [] this->cue_play_on_deck2_buttons;
    delete [] this->cue_del_on_deck2_buttons;
    delete [] this->cue_point_deck2_labels;

    delete [] this->shortcut_set_cue_points;
    delete [] this->shortcut_go_to_cue_points;

    delete this->decks_layout;
}

void
Gui::connect_decks_area()
{
    // Toggle timecode/manual mode.
    QObject::connect(this->deck1_timecode_manual_button, &QPushButton::toggled,
                    [this](bool in_mode)
                    {
                        this->switch_speed_mode(in_mode, 0);
                    });

    // Display speed for each deck.
    for(int i = 0; i < this->nb_decks; i++)
    {
        QObject::connect(this->params[i].data(), &Playback_parameters::speed_changed,
                        [this, i](float in_speed)
                        {
                            this->update_speed_label(in_speed, i);
                        });
    }

    // Speed up/down 0.1%.
    QObject::connect(this->speed_up_on_deck1_button, &SpeedQPushButton::clicked,
                    [this]()
                    {
                        this->speed_up_down(0.001f, 0);
                    });
    QObject::connect(this->speed_down_on_deck1_button, &SpeedQPushButton::clicked,
                    [this]()
                    {
                        speed_up_down(-0.001f, 0);
                    });
    if (this->nb_decks > 1)
    {
        QObject::connect(this->speed_up_on_deck2_button, &SpeedQPushButton::clicked,
                        [this]()
                        {
                            this->speed_up_down(0.001f, 1);
                        });
        QObject::connect(this->speed_down_on_deck2_button, &SpeedQPushButton::clicked,
                        [this]()
                        {
                            speed_up_down(-0.001f, 1);
                        });
    }


    // Speed up/down 1%.
    QObject::connect(this->speed_up_on_deck1_button, &SpeedQPushButton::right_clicked,
                    [this]()
                    {
                        speed_up_down(0.01f, 0);
                    });
    QObject::connect(this->speed_down_on_deck1_button, &SpeedQPushButton::right_clicked,
                    [this]()
                    {
                        speed_up_down(-0.01f, 0);
                    });
    if (this->nb_decks > 1)
    {
        QObject::connect(this->speed_up_on_deck2_button, &SpeedQPushButton::right_clicked,
                        [this]()
                        {
                            speed_up_down(0.01f, 1);
                        });
        QObject::connect(this->speed_down_on_deck2_button, &SpeedQPushButton::right_clicked,
                        [this]()
                        {
                            speed_up_down(-0.01f, 1);
                        });
    }


    // Enable track file dropping in deck group boxes.
    QObject::connect(this->deck1_gbox, &PlaybackQGroupBox::file_dropped, [this](){this->select_and_run_audio_file_decoding_process_deck1();});
    QObject::connect(this->deck2_gbox, &PlaybackQGroupBox::file_dropped, [this](){this->select_and_run_audio_file_decoding_process_deck2();});

    // Remaining time for each deck.
    QObject::connect(this->playback.data(), &Audio_track_playback_process::remaining_time_changed,
                     [this](unsigned int remaining_time, int deck_index)
                     {
                        this->set_remaining_time(remaining_time, deck_index);
                     });

    // Name of the track for each deck.
    QObject::connect(this->ats[0].data(), &Audio_track::name_changed, [this](QString name){this->deck1_track_name->setText(name);});
    QObject::connect(this->ats[1].data(), &Audio_track::name_changed, [this](QString name){this->deck2_track_name->setText(name);});

    // Music key of the track for each deck.
    QObject::connect(this->ats[0].data(), &Audio_track::key_changed, [this](QString key){this->set_deck1_key(key);});
    QObject::connect(this->ats[1].data(), &Audio_track::key_changed, [this](QString key){this->set_deck2_key(key);});

    // Move in track when slider is moved on waveform.
    QObject::connect(this->deck1_waveform, &Waveform::slider_position_changed, [this](float position){this->deck1_jump_to_position(position);});
    QObject::connect(this->deck2_waveform, &Waveform::slider_position_changed, [this](float position){this->deck2_jump_to_position(position);});

    // Keyboard shortcut to go back to the beginning of the track.
    this->shortcut_go_to_begin = new QShortcut(this->window);
    QObject::connect(this->shortcut_go_to_begin,    &QShortcut::activated, [this](){this->deck_go_to_begin();});
    QObject::connect(this->restart_on_deck1_button, &QPushButton::clicked, [this](){this->deck1_go_to_begin();});
    QObject::connect(this->restart_on_deck2_button, &QPushButton::clicked, [this](){this->deck2_go_to_begin();});

    // Keyboard shortcut and buttons to set and play cue points.
    this->shortcut_set_cue_points   = new QShortcut* [MAX_NB_CUE_POINTS];
    this->shortcut_go_to_cue_points = new QShortcut* [MAX_NB_CUE_POINTS];
    for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
    {
        // Shortcut: set cue point.
        this->shortcut_set_cue_points[i] = new QShortcut(this->window);
        QObject::connect(this->shortcut_set_cue_points[i],   &QShortcut::activated, [this, i](){this->deck_set_cue_point(i);});

        // Shortcut: go to cue point.
        this->shortcut_go_to_cue_points[i] = new QShortcut(this->window);
        QObject::connect(this->shortcut_go_to_cue_points[i], &QShortcut::activated, [this, i](){this->deck_go_to_cue_point(i);});

        // Button: set cue point.
        QObject::connect(this->cue_set_on_deck1_buttons[i], &QPushButton::clicked, [this, i](){this->deck1_set_cue_point(i);});

        // Button: go to cue point.
        QObject::connect(this->cue_play_on_deck1_buttons[i], &QPushButton::clicked, [this, i](){this->deck1_go_to_cue_point(i);});

        // Button: del cue point.
        QObject::connect(this->cue_del_on_deck1_buttons[i], &QPushButton::clicked, [this, i](){this->deck1_del_cue_point(i);});
    }
    if (this->nb_decks > 1)
    {
        for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
        {
            // Button: set cue point.
            QObject::connect(this->cue_set_on_deck2_buttons[i], &QPushButton::clicked, [this, i](){this->deck2_set_cue_point(i);});

            // Button: go to cue point.
            QObject::connect(this->cue_play_on_deck2_buttons[i], &QPushButton::clicked, [this, i](){this->deck2_go_to_cue_point(i);});

            // Button: del cue point.
            QObject::connect(this->cue_del_on_deck2_buttons[i], &QPushButton::clicked, [this, i](){this->deck2_del_cue_point(i);});
        }
    }
}

void
Gui::init_samplers_area()
{
    // Init sampler 1 and 2.
    this->init_sampler1_area();
    this->init_sampler2_area();

    // Sampler's show/hide button.
    this->show_hide_samplers_button = new QPushButton();
    this->show_hide_samplers_button->setObjectName("Sampler_show_hide_buttons");
    this->show_hide_samplers_button->setToolTip(tr("Show/hide samplers"));
    this->show_hide_samplers_button->setFixedSize(12, 12);
    this->show_hide_samplers_button->setCheckable(true);
    this->show_hide_samplers_button->setChecked(true);

    // Samplers layout.
    this->samplers_layout = new QHBoxLayout();
    this->samplers_layout->addWidget(this->sampler1_gbox, 100);
    this->samplers_layout->addWidget(this->show_hide_samplers_button, 1, Qt::AlignVCenter);
    if (this->nb_decks > 1)
    {
        this->samplers_layout->addWidget(this->sampler2_gbox, 100);
    }
}

void
Gui::init_sampler1_area()
{
    // Sampler 1 group box
    this->sampler1_buttons_play  = new QPushButton*[this->nb_samplers];
    this->sampler1_buttons_stop  = new QPushButton*[this->nb_samplers];
    this->sampler1_buttons_del   = new QPushButton*[this->nb_samplers];
    this->sampler1_trackname     = new QLabel*[this->nb_samplers];
    this->sampler1_remainingtime = new QLabel*[this->nb_samplers];
    QVBoxLayout *sampler1_layout = new QVBoxLayout();
    this->sampler1_widget        = new QWidget();
    this->sampler1_widget->setLayout(sampler1_layout);
    sampler1_layout->setMargin(0);
    QVBoxLayout *sampler1_layout_container = new QVBoxLayout();
    sampler1_layout_container->addWidget(this->sampler1_widget);
    sampler1_layout->setMargin(0);
    this->sampler1_widget->setObjectName("Sampler_main_widget");
    QString sampler1_name("A");
    for (int i = 0; i < this->nb_samplers; i++)
    {
        this->sampler1_buttons_play[i] = new QPushButton();
        this->sampler1_buttons_play[i]->setObjectName("Sampler_play_buttons");
        this->sampler1_buttons_play[i]->setFixedSize(16, 16);
        this->sampler1_buttons_play[i]->setFocusPolicy(Qt::NoFocus);
        this->sampler1_buttons_play[i]->setCheckable(true);
        this->sampler1_buttons_play[i]->setToolTip(tr("Play sample from start"));
        this->sampler1_buttons_stop[i] = new QPushButton();
        this->sampler1_buttons_stop[i]->setObjectName("Sampler_stop_buttons");
        this->sampler1_buttons_stop[i]->setFixedSize(16, 16);
        this->sampler1_buttons_stop[i]->setFocusPolicy(Qt::NoFocus);
        this->sampler1_buttons_stop[i]->setCheckable(true);
        this->sampler1_buttons_stop[i]->setChecked(true);
        this->sampler1_buttons_stop[i]->setToolTip(tr("Stop sample"));
        this->sampler1_buttons_del[i] = new QPushButton();
        this->sampler1_buttons_del[i]->setObjectName("Sampler_del_buttons");
        this->sampler1_buttons_del[i]->setFixedSize(16, 16);
        this->sampler1_buttons_del[i]->setFocusPolicy(Qt::NoFocus);
        this->sampler1_buttons_del[i]->setCheckable(true);
        this->sampler1_buttons_del[i]->setChecked(true);
        this->sampler1_buttons_del[i]->setToolTip(tr("Delete sample"));
        this->sampler1_trackname[i]     = new QLabel(tr("--"));
        this->sampler1_remainingtime[i] = new QLabel("- 00");

        QSamplerContainerWidget *container = new QSamplerContainerWidget(0, i, this);
        QHBoxLayout *sampler_horz_layout = new QHBoxLayout();
        QLabel *sampler1_name_label = new QLabel(sampler1_name);
        sampler1_name_label->setFixedWidth(15);
        sampler_horz_layout->addWidget(sampler1_name_label,             1);
        sampler_horz_layout->addWidget(this->sampler1_buttons_play[i],  1);
        sampler_horz_layout->addWidget(this->sampler1_buttons_stop[i],  1);
        sampler_horz_layout->addWidget(this->sampler1_buttons_del[i],   1);
        sampler_horz_layout->addWidget(this->sampler1_remainingtime[i], 4);
        sampler_horz_layout->addWidget(this->sampler1_trackname[i],     95);
        sampler_horz_layout->setMargin(0);
        container->setLayout(sampler_horz_layout);
        sampler1_layout->addWidget(container);

        sampler1_name[0].unicode()++; // Next sampler letter.
    }
    this->sampler1_gbox = new PlaybackQGroupBox(tr("Sample player 1"));
    this->sampler1_gbox->setObjectName("SamplerGBox");
    this->sampler1_gbox->setLayout(sampler1_layout_container);
}

void
Gui::init_sampler2_area()
{
    // Sampler 2 group box
    this->sampler2_buttons_play  = new QPushButton*[this->nb_samplers];
    this->sampler2_buttons_stop  = new QPushButton*[this->nb_samplers];
    this->sampler2_buttons_del   = new QPushButton*[this->nb_samplers];
    this->sampler2_trackname     = new QLabel*[this->nb_samplers];
    this->sampler2_remainingtime = new QLabel*[this->nb_samplers];
    QVBoxLayout *sampler2_layout = new QVBoxLayout();
    this->sampler2_widget        = new QWidget();
    this->sampler2_widget->setLayout(sampler2_layout);
    sampler2_layout->setMargin(0);
    QVBoxLayout *sampler2_layout_container = new QVBoxLayout();
    sampler2_layout_container->addWidget(this->sampler2_widget);
    sampler2_layout->setMargin(0);
    this->sampler2_widget->setObjectName("Sampler_main_widget");
    QString sampler2_name("A");
    for (int i = 0; i < this->nb_samplers; i++)
    {
        this->sampler2_buttons_play[i] = new QPushButton();
        this->sampler2_buttons_play[i]->setObjectName("Sampler_play_buttons");
        this->sampler2_buttons_play[i]->setFixedSize(16, 16);
        this->sampler2_buttons_play[i]->setFocusPolicy(Qt::NoFocus);
        this->sampler2_buttons_play[i]->setCheckable(true);
        this->sampler2_buttons_play[i]->setToolTip(tr("Play sample from start"));
        this->sampler2_buttons_stop[i] = new QPushButton();
        this->sampler2_buttons_stop[i]->setObjectName("Sampler_stop_buttons");
        this->sampler2_buttons_stop[i]->setFixedSize(16, 16);
        this->sampler2_buttons_stop[i]->setFocusPolicy(Qt::NoFocus);
        this->sampler2_buttons_stop[i]->setCheckable(true);
        this->sampler2_buttons_stop[i]->setChecked(true);
        this->sampler2_buttons_stop[i]->setToolTip(tr("Stop sample"));
        this->sampler2_buttons_del[i] = new QPushButton();
        this->sampler2_buttons_del[i]->setObjectName("Sampler_del_buttons");
        this->sampler2_buttons_del[i]->setFixedSize(16, 16);
        this->sampler2_buttons_del[i]->setFocusPolicy(Qt::NoFocus);
        this->sampler2_buttons_del[i]->setCheckable(true);
        this->sampler2_buttons_del[i]->setChecked(true);
        this->sampler2_buttons_del[i]->setToolTip(tr("Delete sample"));
        this->sampler2_trackname[i]     = new QLabel(tr("--"));
        this->sampler2_remainingtime[i] = new QLabel("- 00");

        QSamplerContainerWidget *container = new QSamplerContainerWidget(1, i, this);
        QHBoxLayout *sampler_horz_layout = new QHBoxLayout();
        QLabel *sampler2_name_label = new QLabel(sampler2_name);
        sampler2_name_label->setFixedWidth(15);
        sampler_horz_layout->addWidget(sampler2_name_label,             1);
        sampler_horz_layout->addWidget(this->sampler2_buttons_play[i],  1);
        sampler_horz_layout->addWidget(this->sampler2_buttons_stop[i],  1);
        sampler_horz_layout->addWidget(this->sampler2_buttons_del[i],   1);
        sampler_horz_layout->addWidget(this->sampler2_remainingtime[i], 4);
        sampler_horz_layout->addWidget(this->sampler2_trackname[i],     95);
        sampler_horz_layout->setMargin(0);
        container->setLayout(sampler_horz_layout);
        sampler2_layout->addWidget(container);

        sampler2_name[0].unicode()++; // Next sampler letter.
    }
    this->sampler2_gbox = new PlaybackQGroupBox(tr("Sample player 2"));
    this->sampler2_gbox->setObjectName("SamplerGBox");
    this->sampler2_gbox->setLayout(sampler2_layout_container);
}

void
Gui::clean_samplers_area()
{
    // Sampler 1.
    delete [] this->sampler1_buttons_play;
    delete [] this->sampler1_buttons_stop;
    delete [] this->sampler1_buttons_del;
    delete [] this->sampler1_trackname;
    delete [] this->sampler1_remainingtime;
    delete this->sampler1_widget;

    // Sampler 2.
    delete [] this->sampler2_buttons_play;
    delete [] this->sampler2_buttons_stop;
    delete [] this->sampler2_buttons_del;
    delete [] this->sampler2_trackname;
    delete [] this->sampler2_remainingtime;
    delete this->sampler2_widget;
}

void
Gui::connect_samplers_area()
{
    // Show/hide samplers.
    QObject::connect(this->show_hide_samplers_button, &QPushButton::clicked, [this](){this->show_hide_samplers();});

    // Name of samplers.
    for (unsigned short int i = 0; i < this->nb_samplers; i++)
    {
        QObject::connect(this->at_samplers[0][i].data(), &Audio_track::name_changed,
                        [this, i](QString text)
                        {
                            this->set_sampler_1_text(text, i);
                        });
        QObject::connect(this->at_samplers[1][i].data(), &Audio_track::name_changed,
                        [this, i](QString text)
                        {
                            this->set_sampler_2_text(text, i);
                        });
    }

    // Start stop samplers.
    for (unsigned short int i = 0; i < this->nb_samplers; i++)
    {
        // Button: play sampler.
        QObject::connect(this->sampler1_buttons_play[i], &QPushButton::clicked,
                [this, i](){this->on_sampler_button_play_click(0, i);});
        QObject::connect(this->sampler2_buttons_play[i], &QPushButton::clicked,
                [this, i](){this->on_sampler_button_play_click(1, i);});

        // Button: stop sampler.
        QObject::connect(this->sampler1_buttons_stop[i], &QPushButton::clicked,
                [this, i](){this->on_sampler_button_stop_click(0, i);});
        QObject::connect(this->sampler2_buttons_stop[i], &QPushButton::clicked,
                [this, i](){this->on_sampler_button_stop_click(1, i);});

        // Button: del sampler.
        QObject::connect(this->sampler1_buttons_del[i], &QPushButton::clicked,
                [this, i](){this->on_sampler_button_del_click(0, i);});
        QObject::connect(this->sampler2_buttons_del[i], &QPushButton::clicked,
                [this, i](){this->on_sampler_button_del_click(1, i);});
    }

    // Remaining time for samplers.
    QObject::connect(this->playback.data(), &Audio_track_playback_process::sampler_remaining_time_changed,
                    [this](unsigned int remaining_time, int deck_index, int sampler_index)
                    {
                        this->set_sampler_remaining_time(remaining_time, deck_index, sampler_index);
                    });

    // State for samplers.
    QObject::connect(this->playback.data(), &Audio_track_playback_process::sampler_state_changed,
                    [this](int deck_index, int sampler_index, bool state)
                    {
                        this->set_sampler_state(deck_index, sampler_index, state);
                    });
}

void
Gui::connect_decks_and_samplers_selection()
{
    // Create mouse action to select deck/sampler.
    QObject::connect(this->deck1_gbox, SIGNAL(selected()), this, SLOT(select_playback_1()));
    QObject::connect(this->deck2_gbox, SIGNAL(selected()), this, SLOT(select_playback_2()));
    QObject::connect(this->sampler1_gbox, SIGNAL(selected()), this, SLOT(select_playback_1()));
    QObject::connect(this->sampler2_gbox, SIGNAL(selected()), this, SLOT(select_playback_2()));

    // Create mouse action when deck/sampler is hovered (mouse entering area).
    QSignalMapper *deck_enter_signal_mapper = new QSignalMapper(this);
    deck_enter_signal_mapper->setMapping(this->deck1_gbox, 0);
    QObject::connect(this->deck1_gbox, SIGNAL(hover()), deck_enter_signal_mapper, SLOT(map()));
    deck_enter_signal_mapper->setMapping(this->deck2_gbox, 1);
    QObject::connect(this->deck2_gbox, SIGNAL(hover()), deck_enter_signal_mapper, SLOT(map()));
    QObject::connect(deck_enter_signal_mapper, SIGNAL(mapped(int)), this, SLOT(hover_playback(int)));

    QSignalMapper *sampler_enter_signal_mapper = new QSignalMapper(this);
    sampler_enter_signal_mapper->setMapping(this->sampler1_gbox, 0);
    QObject::connect(this->sampler1_gbox, SIGNAL(hover()), sampler_enter_signal_mapper, SLOT(map()));
    sampler_enter_signal_mapper->setMapping(this->sampler2_gbox, 1);
    QObject::connect(this->sampler2_gbox, SIGNAL(hover()), sampler_enter_signal_mapper, SLOT(map()));
    QObject::connect(sampler_enter_signal_mapper, SIGNAL(mapped(int)), this, SLOT(hover_playback(int)));

    // Create mouse action when deck/sampler is unhovered (mouse leaving area).
    QSignalMapper *deck_leave_signal_mapper = new QSignalMapper(this);
    deck_leave_signal_mapper->setMapping(this->deck1_gbox, 0);
    QObject::connect(this->deck1_gbox, SIGNAL(unhover()), deck_leave_signal_mapper, SLOT(map()));
    deck_leave_signal_mapper->setMapping(this->deck2_gbox, 1);
    QObject::connect(this->deck2_gbox, SIGNAL(unhover()), deck_leave_signal_mapper, SLOT(map()));
    QObject::connect(deck_leave_signal_mapper, SIGNAL(mapped(int)), this, SLOT(unhover_playback(int)));

    QSignalMapper *sampler_leave_signal_mapper = new QSignalMapper(this);
    sampler_leave_signal_mapper->setMapping(this->sampler1_gbox, 0);
    QObject::connect(this->sampler1_gbox, SIGNAL(unhover()), sampler_leave_signal_mapper, SLOT(map()));
    sampler_leave_signal_mapper->setMapping(this->sampler2_gbox, 1);
    QObject::connect(this->sampler2_gbox, SIGNAL(unhover()), sampler_leave_signal_mapper, SLOT(map()));
    QObject::connect(sampler_leave_signal_mapper, SIGNAL(mapped(int)), this, SLOT(unhover_playback(int)));

    // Preselect deck and sampler 1.
    this->deck1_gbox->setProperty("selected", true);
    this->sampler1_gbox->setProperty("selected", true);

    // Connect keyboard shortcut to switch selection of decks/samplers.
    this->shortcut_switch_playback = new QShortcut(this->window);
    QObject::connect(this->shortcut_switch_playback, SIGNAL(activated()), this, SLOT(switch_playback_selection()));
}

void
Gui::init_file_browser_area()
{
    // Create the folder browser.
    this->treeview_icon_provider = new TreeViewIconProvider();
    this->folder_system_model    = new QFileSystemModel();
    this->folder_system_model->setIconProvider(this->treeview_icon_provider);
    this->folder_browser         = new QTreeView();
    this->folder_browser->setModel(this->folder_system_model);
    this->folder_browser->setColumnHidden(1, true);
    this->folder_browser->setColumnHidden(2, true);
    this->folder_browser->setColumnHidden(3, true);
    this->folder_browser->setHeaderHidden(true);
    this->folder_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->folder_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Create the file browser (track browser).
    this->file_system_model = new Audio_collection_model();
    this->file_browser      = new QTreeView();
    this->file_browser->setModel(this->file_system_model);
    this->file_browser->setSelectionMode(QAbstractItemView::SingleSelection);
    this->file_browser->setDragEnabled(true);
    this->file_browser->setDragDropMode(QAbstractItemView::DragOnly);
    this->file_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->file_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->file_browser->header()->setSortIndicatorShown(true);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    this->file_browser->header()->setClickable(true);
#else
    this->file_browser->header()->setSectionsClickable(true);
#endif

    // Create the track search bar.
    this->file_search                 = new QLineEdit();
    this->search_from_begin           = false;
    this->file_search->setPlaceholderText(tr("Search..."));
    this->file_browser_selected_index = 0;    

    // Create function buttons for file browser.
    this->refresh_file_browser = new QPushButton();
    this->refresh_file_browser->setObjectName("Refresh_browser_button");
    this->refresh_file_browser->setToolTip(tr("Analyze audio collection (get musical key)"));
    this->refresh_file_browser->setFixedSize(24, 24);
    this->refresh_file_browser->setFocusPolicy(Qt::NoFocus);
    this->refresh_file_browser->setCheckable(true);

    this->load_track_on_deck1_button = new QPushButton();
    this->load_track_on_deck1_button->setObjectName("Load_track_button_1");
    this->load_track_on_deck1_button->setToolTip("<p>" + tr("Load selected track to deck 1") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_DECK) + "</em>");
    this->load_track_on_deck1_button->setFixedSize(24, 24);
    this->load_track_on_deck1_button->setFocusPolicy(Qt::NoFocus);
    this->load_track_on_deck1_button->setCheckable(true);

    this->show_next_key_from_deck1_button = new QPushButton();
    this->show_next_key_from_deck1_button->setObjectName("Show_next_key_button");
    this->show_next_key_from_deck1_button->setToolTip("<p>" + tr("Show deck 1 next potential tracks") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_SHOW_NEXT_KEYS) + "</em>");
    this->show_next_key_from_deck1_button->setFixedSize(24, 24);
    this->show_next_key_from_deck1_button->setFocusPolicy(Qt::NoFocus);
    this->show_next_key_from_deck1_button->setCheckable(true);

    this->load_sample1_1_button = new QPushButton();
    this->load_sample1_1_button->setObjectName("Load_track_sample_button_a");
    this->load_sample1_1_button->setToolTip("<p>" + tr("Load selected track to sample A") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER1) + "</em>");
    this->load_sample1_1_button->setFixedSize(20, 20);
    this->load_sample1_1_button->setFocusPolicy(Qt::NoFocus);
    this->load_sample1_1_button->setCheckable(true);

    this->load_sample1_2_button = new QPushButton();
    this->load_sample1_2_button->setObjectName("Load_track_sample_button_b");
    this->load_sample1_2_button->setToolTip("<p>" + tr("Load selected track to sample B") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER2) + "</em>");
    this->load_sample1_2_button->setFixedSize(20, 20);
    this->load_sample1_2_button->setFocusPolicy(Qt::NoFocus);
    this->load_sample1_2_button->setCheckable(true);

    this->load_sample1_3_button = new QPushButton();
    this->load_sample1_3_button->setObjectName("Load_track_sample_button_c");
    this->load_sample1_3_button->setToolTip("<p>" + tr("Load selected track to sample C") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER3) + "</em>");
    this->load_sample1_3_button->setFixedSize(20, 20);
    this->load_sample1_3_button->setFocusPolicy(Qt::NoFocus);
    this->load_sample1_3_button->setCheckable(true);

    this->load_sample1_4_button = new QPushButton();
    this->load_sample1_4_button->setObjectName("Load_track_sample_button_d");
    this->load_sample1_4_button->setToolTip("<p>" + tr("Load selected track to sample D") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER4) + "</em>");
    this->load_sample1_4_button->setFixedSize(20, 20);
    this->load_sample1_4_button->setFocusPolicy(Qt::NoFocus);
    this->load_sample1_4_button->setCheckable(true);

    if (this->nb_decks > 1)
    {
        this->load_track_on_deck2_button = new QPushButton();
        this->load_track_on_deck2_button->setObjectName("Load_track_button_2");
        this->load_track_on_deck2_button->setToolTip("<p>" + tr("Load selected track to deck 2") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_DECK) + "</em>");
        this->load_track_on_deck2_button->setFixedSize(24, 24);
        this->load_track_on_deck2_button->setFocusPolicy(Qt::NoFocus);
        this->load_track_on_deck2_button->setCheckable(true);

        this->show_next_key_from_deck2_button = new QPushButton();
        this->show_next_key_from_deck2_button->setObjectName("Show_next_key_button");
        this->show_next_key_from_deck2_button->setToolTip("<p>" + tr("Show deck 2 next potential tracks") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_SHOW_NEXT_KEYS) + "</em>");
        this->show_next_key_from_deck2_button->setFixedSize(24, 24);
        this->show_next_key_from_deck2_button->setFocusPolicy(Qt::NoFocus);
        this->show_next_key_from_deck2_button->setCheckable(true);

        this->load_sample2_1_button = new QPushButton();
        this->load_sample2_1_button->setObjectName("Load_track_sample_button_a");
        this->load_sample2_1_button->setToolTip("<p>" + tr("Load selected track to sample A") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER1) + "</em>");
        this->load_sample2_1_button->setFixedSize(20, 20);
        this->load_sample2_1_button->setFocusPolicy(Qt::NoFocus);
        this->load_sample2_1_button->setCheckable(true);

        this->load_sample2_2_button = new QPushButton();
        this->load_sample2_2_button->setObjectName("Load_track_sample_button_b");
        this->load_sample2_2_button->setToolTip("<p>" + tr("Load selected track to sample B") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER2) + "</em>");
        this->load_sample2_2_button->setFixedSize(20, 20);
        this->load_sample2_2_button->setFocusPolicy(Qt::NoFocus);
        this->load_sample2_2_button->setCheckable(true);        

        this->load_sample2_3_button = new QPushButton();
        this->load_sample2_3_button->setObjectName("Load_track_sample_button_c");
        this->load_sample2_3_button->setToolTip("<p>" + tr("Load selected track to sample C") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER3) + "</em>");
        this->load_sample2_3_button->setFixedSize(20, 20);
        this->load_sample2_3_button->setFocusPolicy(Qt::NoFocus);
        this->load_sample2_3_button->setCheckable(true);

        this->load_sample2_4_button = new QPushButton();
        this->load_sample2_4_button->setObjectName("Load_track_sample_button_d");
        this->load_sample2_4_button->setToolTip("<p>" + tr("Load selected track to sample D") + "</p><em>" + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER4) + "</em>");
        this->load_sample2_4_button->setFixedSize(20, 20);
        this->load_sample2_4_button->setFocusPolicy(Qt::NoFocus);
        this->load_sample2_4_button->setCheckable(true);
    }

    // File browser buttons.
    QWidget *file_browser_buttons_widget = new QWidget();
    QGridLayout *file_browser_buttons_layout         = new QGridLayout(file_browser_buttons_widget);
    QHBoxLayout *file_browser_sample1_buttons_layout = new QHBoxLayout();
    file_browser_sample1_buttons_layout->addWidget(this->load_sample1_1_button);
    file_browser_sample1_buttons_layout->addWidget(this->load_sample1_2_button);
    file_browser_sample1_buttons_layout->addWidget(this->load_sample1_3_button);
    file_browser_sample1_buttons_layout->addWidget(this->load_sample1_4_button);
    QHBoxLayout *file_browser_sample2_buttons_layout = new QHBoxLayout();
    if (this->nb_decks > 1)
    {
        file_browser_sample2_buttons_layout->addWidget(this->load_sample2_1_button);
        file_browser_sample2_buttons_layout->addWidget(this->load_sample2_2_button);
        file_browser_sample2_buttons_layout->addWidget(this->load_sample2_3_button);
        file_browser_sample2_buttons_layout->addWidget(this->load_sample2_4_button);
    }
    file_browser_buttons_layout->addWidget(this->load_track_on_deck1_button,     0, 0, Qt::AlignLeft);
    file_browser_buttons_layout->addWidget(this->show_next_key_from_deck1_button,0, 1, Qt::AlignLeft);
    file_browser_buttons_layout->addLayout(file_browser_sample1_buttons_layout,  0, 2, Qt::AlignRight);
    file_browser_buttons_layout->addWidget(this->refresh_file_browser,           0, 3, Qt::AlignCenter);
    if (this->nb_decks > 1)
    {
        file_browser_buttons_layout->addLayout(file_browser_sample2_buttons_layout,  0, 4, Qt::AlignRight);
        file_browser_buttons_layout->addWidget(this->show_next_key_from_deck2_button,0, 5, Qt::AlignRight);
        file_browser_buttons_layout->addWidget(this->load_track_on_deck2_button,     0, 6, Qt::AlignRight);
    }
    file_browser_buttons_layout->setColumnStretch(0, 1);
    file_browser_buttons_layout->setColumnStretch(1, 100);
    file_browser_buttons_layout->setColumnStretch(2, 1);
    file_browser_buttons_layout->setColumnStretch(3, 10);
    if (this->nb_decks > 1)
    {
        file_browser_buttons_layout->setColumnStretch(4, 1);
        file_browser_buttons_layout->setColumnStretch(5, 100);
        file_browser_buttons_layout->setColumnStretch(6, 1);
    }
    file_browser_buttons_widget->setFixedHeight(37);

    // Create layout and group box for file browser.
    QVBoxLayout *file_browser_layout = new QVBoxLayout();
    file_browser_layout->addWidget(file_browser_buttons_widget);

    QFrame* horiz_line = new QFrame();
    horiz_line->setFrameShape(QFrame::HLine);
    horiz_line->setObjectName("Horizontal_line");
    file_browser_layout->addWidget(horiz_line);

    QVBoxLayout *file_browser_and_search_layout = new QVBoxLayout();
    file_browser_and_search_layout->addWidget(this->file_browser);
    file_browser_and_search_layout->addWidget(this->file_search, 0, Qt::AlignBottom);
    file_browser_and_search_layout->setMargin(0);
    QWidget *file_browser_and_search_widget = new QWidget();
    file_browser_and_search_widget->setLayout(file_browser_and_search_layout);

    this->browser_splitter = new QSplitter();
    this->browser_splitter->addWidget(this->folder_browser);
    this->browser_splitter->addWidget(file_browser_and_search_widget);
    this->browser_splitter->setStretchFactor(0, 1);
    this->browser_splitter->setStretchFactor(1, 4);
    file_browser_layout->addWidget(this->browser_splitter);

    this->file_browser_gbox = new QGroupBox();
    this->set_file_browser_title(this->settings->get_tracks_base_dir_path());
    this->file_browser_gbox->setLayout(file_browser_layout);

    this->file_layout = new QHBoxLayout();
    this->file_layout->addWidget(this->file_browser_gbox, 50);
}

void
Gui::clean_file_browser_area()
{
    delete this->watcher_parse_directory;
    delete this->treeview_icon_provider;
    delete this->folder_system_model;
    delete this->folder_browser;
    delete this->file_system_model;
    delete this->file_browser;
    delete this->file_layout;
}

void
Gui::connect_file_browser_area()
{
    // Refresh track browser.
    QObject::connect(this->refresh_file_browser, SIGNAL(clicked()), this, SLOT(show_refresh_audio_collection_dialog()));

    // Load track on deck.
    QObject::connect(this->load_track_on_deck1_button, SIGNAL(clicked()), this, SLOT(select_and_run_audio_file_decoding_process_deck1()));
    if (this->nb_decks > 1)
        QObject::connect(this->load_track_on_deck2_button, SIGNAL(clicked()), this, SLOT(select_and_run_audio_file_decoding_process_deck2()));

    // Show next tracks (based on music key).
    QObject::connect(this->show_next_key_from_deck1_button, SIGNAL(clicked()), this, SLOT(select_and_show_next_keys_deck1()));
    if (this->nb_decks > 1)
        QObject::connect(this->show_next_key_from_deck2_button, SIGNAL(clicked()), this, SLOT(select_and_show_next_keys_deck2()));

    // Open folder or playlist from file browser on double click.
    QObject::connect(this->folder_browser, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(on_file_browser_double_click(QModelIndex)));

    // Resize column with file name when expanding/collapsing a directory.
    QObject::connect(this->file_browser, SIGNAL(expanded(QModelIndex)),  this, SLOT(on_file_browser_expand(QModelIndex)));
    QObject::connect(this->file_browser, SIGNAL(collapsed(QModelIndex)), this, SLOT(on_file_browser_expand(QModelIndex)));

    // Connect the keyboard shortcut that collapse tree.
    this->shortcut_collapse_browser = new QShortcut(this->file_browser);
    QObject::connect(this->shortcut_collapse_browser, SIGNAL(activated()), this->file_browser, SLOT(collapseAll()));

    // Connect the keyboard shortcut to start decoding process on selected file.
    this->shortcut_load_audio_file = new QShortcut(this->file_browser);
    QObject::connect(this->shortcut_load_audio_file, SIGNAL(activated()), this, SLOT(run_audio_file_decoding_process()));

    // Sort track browser when clicking on header.
    QObject::connect(this->file_browser->header(), SIGNAL(sectionClicked(int)), this, SLOT(on_file_browser_header_click(int)));

    // Connect the keyboard shortcut to show next audio file according to current music key.
    this->shortcut_show_next_keys = new QShortcut(this->file_browser);
    QObject::connect(this->shortcut_show_next_keys, SIGNAL(activated()), this, SLOT(show_next_keys()));

    // Load track in sampler.
    this->shortcut_load_sample_file_1 = new QShortcut(this->file_browser);
    this->shortcut_load_sample_file_2 = new QShortcut(this->file_browser);
    this->shortcut_load_sample_file_3 = new QShortcut(this->file_browser);
    this->shortcut_load_sample_file_4 = new QShortcut(this->file_browser);
    QObject::connect(this->shortcut_load_sample_file_1, SIGNAL(activated()), this, SLOT(run_sample_1_decoding_process()));
    QObject::connect(this->shortcut_load_sample_file_2, SIGNAL(activated()), this, SLOT(run_sample_2_decoding_process()));
    QObject::connect(this->shortcut_load_sample_file_3, SIGNAL(activated()), this, SLOT(run_sample_3_decoding_process()));
    QObject::connect(this->shortcut_load_sample_file_4, SIGNAL(activated()), this, SLOT(run_sample_4_decoding_process()));
    QObject::connect(this->load_sample1_1_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample1_decoding_process_deck1()));
    QObject::connect(this->load_sample1_2_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample2_decoding_process_deck1()));
    QObject::connect(this->load_sample1_3_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample3_decoding_process_deck1()));
    QObject::connect(this->load_sample1_4_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample4_decoding_process_deck1()));
    if (this->nb_decks > 1)
    {
        QObject::connect(this->load_sample2_1_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample1_decoding_process_deck2()));
        QObject::connect(this->load_sample2_2_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample2_decoding_process_deck2()));
        QObject::connect(this->load_sample2_3_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample3_decoding_process_deck2()));
        QObject::connect(this->load_sample2_4_button, SIGNAL(clicked()), this, SLOT(select_and_run_sample4_decoding_process_deck2()));
    }

    // Connect thread states for audio collection read and write to DB.
    QObject::connect(this->file_system_model->concurrent_watcher_read.data(),  SIGNAL(finished()), this, SLOT(sync_file_browser_to_audio_collection()));
    QObject::connect(this->file_system_model->concurrent_watcher_store.data(), SIGNAL(finished()), this, SLOT(on_finished_analyze_audio_collection()));

    // Add context menu for file browser (load track and samples).
    QAction *load_on_deck_1_action = new QAction(tr("Load on deck 1"), this);
    load_on_deck_1_action->setStatusTip(tr("Load selected track to deck 1"));
    connect(load_on_deck_1_action, SIGNAL(triggered()), this, SLOT(select_and_run_audio_file_decoding_process_deck1()));
    QAction *load_on_deck_2_action = new QAction(tr("Load on deck 2"), this);
    if (this->nb_decks > 1)
    {
        load_on_deck_2_action->setStatusTip(tr("Load selected track to deck 2"));
        connect(load_on_deck_2_action, SIGNAL(triggered()), this, SLOT(select_and_run_audio_file_decoding_process_deck2()));
    }
    QAction *load_on_sampler_1A_action = new QAction(tr("Sampler A"), this);
    load_on_sampler_1A_action->setStatusTip(tr("Load selected track to sampler A"));
    connect(load_on_sampler_1A_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample1_decoding_process_deck1()));
    QAction *load_on_sampler_1B_action = new QAction(tr("Sampler B"), this);
    load_on_sampler_1B_action->setStatusTip(tr("Load selected track to sampler B"));
    connect(load_on_sampler_1B_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample2_decoding_process_deck1()));
    QAction *load_on_sampler_1C_action = new QAction(tr("Sampler C"), this);
    load_on_sampler_1C_action->setStatusTip(tr("Load selected track to sampler C"));
    connect(load_on_sampler_1C_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample3_decoding_process_deck1()));
    QAction *load_on_sampler_1D_action = new QAction(tr("Sampler D"), this);
    load_on_sampler_1D_action->setStatusTip(tr("Load selected track to sampler D"));
    connect(load_on_sampler_1D_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample4_decoding_process_deck1()));
    QAction *load_on_sampler_1_action = new QAction(tr("Load on sampler 1"), this);
    load_on_sampler_1_action->setStatusTip(tr("Load selected track to sampler 1"));
    QMenu *load_on_sampler_1_submenu = new QMenu(this->file_browser);
    load_on_sampler_1_action->setMenu(load_on_sampler_1_submenu);
    load_on_sampler_1_submenu->addAction(load_on_sampler_1A_action);
    load_on_sampler_1_submenu->addAction(load_on_sampler_1B_action);
    load_on_sampler_1_submenu->addAction(load_on_sampler_1C_action);
    load_on_sampler_1_submenu->addAction(load_on_sampler_1D_action);

    QAction *load_on_sampler_2_action = new QAction(tr("Load on sampler 2"), this);
    if (this->nb_decks > 1)
    {
        QAction *load_on_sampler_2A_action = new QAction(tr("Sampler A"), this);
        load_on_sampler_2A_action->setStatusTip(tr("Load selected track to sampler A"));
        connect(load_on_sampler_2A_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample1_decoding_process_deck2()));
        QAction *load_on_sampler_2B_action = new QAction(tr("Sampler B"), this);
        load_on_sampler_2B_action->setStatusTip(tr("Load selected track to sampler B"));
        connect(load_on_sampler_2B_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample2_decoding_process_deck2()));
        QAction *load_on_sampler_2C_action = new QAction(tr("Sampler C"), this);
        load_on_sampler_2C_action->setStatusTip(tr("Load selected track to sampler C"));
        connect(load_on_sampler_2C_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample3_decoding_process_deck2()));
        QAction *load_on_sampler_2D_action = new QAction(tr("Sampler D"), this);
        load_on_sampler_2D_action->setStatusTip(tr("Load selected track to sampler D"));
        connect(load_on_sampler_2D_action, SIGNAL(triggered()), this, SLOT(select_and_run_sample4_decoding_process_deck2()));
        load_on_sampler_2_action->setStatusTip(tr("Load selected track to sampler 2"));
        QMenu *load_on_sampler_2_submenu = new QMenu(this->file_browser);
        load_on_sampler_2_action->setMenu(load_on_sampler_2_submenu);
        load_on_sampler_2_submenu->addAction(load_on_sampler_2A_action);
        load_on_sampler_2_submenu->addAction(load_on_sampler_2B_action);
        load_on_sampler_2_submenu->addAction(load_on_sampler_2C_action);
        load_on_sampler_2_submenu->addAction(load_on_sampler_2D_action);
    }

    this->file_browser->setContextMenuPolicy(Qt::ActionsContextMenu);
    this->file_browser->addAction(load_on_deck_1_action);
    if (this->nb_decks > 1)
    {
        this->file_browser->addAction(load_on_deck_2_action);
    }
    this->file_browser->addAction(load_on_sampler_1_action);
    if (this->nb_decks > 1)
    {
        this->file_browser->addAction(load_on_sampler_2_action);
    }

    // Search bar for file browser.
    this->shortcut_file_search             = new QShortcut(this->window);
    this->shortcut_file_search_press_enter = new QShortcut(this->file_search);
    this->shortcut_file_search_press_esc   = new QShortcut(this->file_search);
    QObject::connect(this->shortcut_file_search, SIGNAL(activated()), this, SLOT(set_focus_search_bar()));
    QObject::connect(this->file_search, SIGNAL(textChanged(QString)), this, SLOT(file_search_string(QString)));
    QObject::connect(this->shortcut_file_search_press_enter, SIGNAL(activated()), this, SLOT(press_enter_in_search_bar()));
    QObject::connect(this->file_search, SIGNAL(returnPressed()), this, SLOT(press_enter_in_search_bar()));
    QObject::connect(this->shortcut_file_search_press_esc, SIGNAL(activated()), this, SLOT(press_esc_in_search_bar()));

    // Progress for file analyzis and storage.
    QObject::connect(this->file_system_model->concurrent_watcher_store.data(), SIGNAL(progressRangeChanged(int,int)),
                     this->progress_bar,                                       SLOT(setRange(int,int)));
    QObject::connect(this->file_system_model->concurrent_watcher_store.data(), SIGNAL(progressValueChanged(int)),
                     this,                                                     SLOT(update_refresh_progress_value(int)));

    // Progress for reading file data from storage.
    QObject::connect(this->file_system_model->concurrent_watcher_read.data(), SIGNAL(progressRangeChanged(int,int)),
                     this->progress_bar,                                      SLOT(setRange(int,int)));
    QObject::connect(this->file_system_model->concurrent_watcher_read.data(), SIGNAL(progressValueChanged(int)),
                     this,                                                    SLOT(update_refresh_progress_value(int)));

    // Parse directory thread.
    this->watcher_parse_directory = new QFutureWatcher<void>;
    connect(this->watcher_parse_directory, SIGNAL(finished()),
            this,                          SLOT(run_concurrent_read_collection_from_db()));
}

void
Gui::init_bottom_help()
{
    // Create help labels and pixmaps.
    QLabel *help_display_lb        = new QLabel(tr("Display"));
    QLabel *help_fullscreen_lb     = new QLabel(tr("Fullscreen"));
    this->help_fullscreen_value    = new QLabel();
    QLabel *help_help_lb           = new QLabel(tr("Help"));
    this->help_help_value          = new QLabel();
    QLabel *help_switch_deck_lb    = new QLabel(tr("Switch selected playback"));
    this->help_switch_deck_value   = new QLabel();

    QLabel *help_deck_lb           = new QLabel(tr("Selected deck"));
    QLabel *help_load_deck_lb      = new QLabel(tr("Load/Restart track"));
    this->help_load_deck_value     = new QLabel();
    QLabel *help_next_track_lb     = new QLabel(tr("Highlight next tracks"));
    this->help_next_track_value    = new QLabel();
    QLabel *help_cue_lb            = new QLabel(tr("Set/Play cue point 1/2/3/4"));
    this->help_cue_value           = new QLabel();

    QLabel *help_sampler_lb        = new QLabel(tr("Selected sampler"));
    QLabel *help_sample_lb         = new QLabel(tr("Load sampler 1/2/3/4"));
    this->help_sample_value        = new QLabel();
    QLabel *help_online_lb         = new QLabel(tr("Online wiki help"));
    QLabel *help_url_lb            = new QLabel("<a style=\"color: white\" href=\"https://github.com/jrosener/digitalscratch/wiki\">https://github.com/jrosener/digitalscratch/wiki</a>");
    help_url_lb->setTextFormat(Qt::RichText);
    help_url_lb->setTextInteractionFlags(Qt::TextBrowserInteraction);
    help_url_lb->setOpenExternalLinks(true);

    QLabel *help_browser_lb        = new QLabel(tr("File browser"));
    QLabel *help_browse_lb1        = new QLabel(tr("Browse"));
    this->help_browse_value1       = new QLabel();
    QLabel *help_browse_lb2        = new QLabel(tr("Collapse all"));
    this->help_browse_value2       = new QLabel();

    this->set_help_shortcut_value();

    help_display_lb->setObjectName("Help_title");
    help_fullscreen_lb->setObjectName("Help");
    help_help_lb->setObjectName("Help");
    help_switch_deck_lb->setObjectName("Help");

    help_deck_lb->setObjectName("Help_title");
    help_load_deck_lb->setObjectName("Help");
    help_next_track_lb->setObjectName("Help");
    help_cue_lb->setObjectName("Help");

    help_sampler_lb->setObjectName("Help_title");
    help_sample_lb->setObjectName("Help");
    help_online_lb->setObjectName("Help_title");
    help_url_lb->setObjectName("Help_url");

    help_browser_lb->setObjectName("Help_title");
    help_browse_lb1->setObjectName("Help");
    help_browse_lb2->setObjectName("Help");

    // Main help layout.
    QGridLayout *help_layout = new QGridLayout();

    help_layout->addWidget(help_display_lb,           0, 0);
    help_layout->addWidget(help_fullscreen_lb,        1, 0);
    help_layout->addWidget(help_fullscreen_value,     1, 1, Qt::AlignLeft);
    help_layout->addWidget(help_help_lb,              2, 0);
    help_layout->addWidget(help_help_value,           2, 1, Qt::AlignLeft);
    help_layout->addWidget(help_switch_deck_lb,       3, 0);
    help_layout->addWidget(help_switch_deck_value,    3, 1, Qt::AlignLeft);

    help_layout->addWidget(help_deck_lb,              0, 2);
    help_layout->addWidget(help_load_deck_lb,         1, 2);
    help_layout->addWidget(help_load_deck_value,      1, 3, Qt::AlignLeft);
    help_layout->addWidget(help_next_track_lb,        2, 2);
    help_layout->addWidget(help_next_track_value,     2, 3, Qt::AlignLeft);
    help_layout->addWidget(help_cue_lb,               3, 2);
    help_layout->addWidget(help_cue_value,            3, 3, Qt::AlignLeft);

    help_layout->addWidget(help_sampler_lb,           0, 4);
    help_layout->addWidget(help_sample_lb,            1, 4);
    help_layout->addWidget(help_sample_value,         1, 5, Qt::AlignLeft);
    help_layout->addWidget(help_online_lb,            3, 4);
    help_layout->addWidget(help_url_lb,               3, 5, 1, 3, Qt::AlignLeft);

    help_layout->addWidget(help_browser_lb,           0, 6);
    help_layout->addWidget(help_browse_lb1,           1, 6);
    help_layout->addWidget(help_browse_value1,        1, 7, Qt::AlignLeft);
    help_layout->addWidget(help_browse_lb2,           2, 6);
    help_layout->addWidget(help_browse_value2,        2, 7, Qt::AlignLeft);

    help_layout->setColumnStretch(0, 1);
    help_layout->setColumnStretch(1, 5);
    help_layout->setColumnStretch(2, 1);
    help_layout->setColumnStretch(3, 5);
    help_layout->setColumnStretch(4, 1);
    help_layout->setColumnStretch(5, 5);
    help_layout->setColumnStretch(6, 1);
    help_layout->setColumnStretch(7, 5);

    // Create help group box.
    this->help_groupbox = new QGroupBox();
    this->help_groupbox->hide();
    this->help_groupbox->setObjectName("Help");

    // Put help horizontal layout in help group box.
    this->help_groupbox->setLayout(help_layout);

    // Create bottom horizontal layout.
    this->bottom_layout = new QHBoxLayout;

    // Put help group box and configuration in bottom layout.
    this->bottom_layout->addWidget(this->help_groupbox);
}

void
Gui::clean_bottom_help()
{
    delete this->bottom_layout;
}

void
Gui::init_bottom_status()
{
    // Create groupbox for progress bar.
    this->progress_groupbox = new QGroupBox();
    this->progress_groupbox->hide();
    this->progress_groupbox->setObjectName("Progress");

    // Create progress bar.
    this->progress_bar = new QProgressBar(this->progress_groupbox);
    this->progress_bar->setObjectName("Progress");

    // Create cancel button.
    this->progress_cancel_button = new QPushButton(this->progress_groupbox);
    this->progress_cancel_button->setObjectName("Progress");
    this->progress_cancel_button->setFixedSize(16, 16);
    this->progress_cancel_button->setFocusPolicy(Qt::NoFocus);
    this->progress_cancel_button->setToolTip(tr("Cancel execution"));
    QObject::connect(this->progress_cancel_button, SIGNAL(clicked()), this, SLOT(on_progress_cancel_button_click()));

    // Create label.
    this->progress_label = new QLabel(this->progress_groupbox);
    this->progress_label->setObjectName("Progress");

    // Create layout for progress bar.
    QHBoxLayout *progress_layout = new QHBoxLayout;
    progress_layout->addWidget(this->progress_bar);
    progress_layout->addWidget(this->progress_cancel_button);
    progress_layout->addWidget(this->progress_label);
    this->progress_groupbox->setLayout(progress_layout);
    this->status_layout = new QHBoxLayout;
    this->status_layout->addWidget(this->progress_groupbox);
}

void
Gui::clean_bottom_status()
{
    delete this->status_layout;
}

void
Gui::display_audio_file_collection()
{
    QCoreApplication::processEvents();
    this->set_file_browser_base_path(this->settings->get_tracks_base_dir_path());
    this->is_window_rendered = true;
}

void
Gui::set_help_shortcut_value()
{
    this->help_fullscreen_value->setText(this->settings->get_keyboard_shortcut(KB_FULLSCREEN));
    this->help_help_value->setText(this->settings->get_keyboard_shortcut(KB_HELP));
    this->help_switch_deck_value->setText(this->settings->get_keyboard_shortcut(KB_SWITCH_PLAYBACK));
    this->help_load_deck_value->setText(this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_DECK)
                                        + "/"
                                        + this->settings->get_keyboard_shortcut(KB_PLAY_BEGIN_TRACK_ON_DECK));
    this->help_next_track_value->setText(this->settings->get_keyboard_shortcut(KB_SHOW_NEXT_KEYS));
    this->help_cue_value->setText(this->settings->get_keyboard_shortcut(KB_SET_CUE_POINT1_ON_DECK)
                                  + "/"
                                  + this->settings->get_keyboard_shortcut(KB_PLAY_CUE_POINT1_ON_DECK)
                                  + ", " + this->settings->get_keyboard_shortcut(KB_SET_CUE_POINT2_ON_DECK)
                                  + "/"
                                  + this->settings->get_keyboard_shortcut(KB_PLAY_CUE_POINT2_ON_DECK)
                                  + ", " + this->settings->get_keyboard_shortcut(KB_SET_CUE_POINT3_ON_DECK)
                                  + "/"
                                  + this->settings->get_keyboard_shortcut(KB_PLAY_CUE_POINT3_ON_DECK)
                                  + ", " + this->settings->get_keyboard_shortcut(KB_SET_CUE_POINT4_ON_DECK)
                                  + "/"
                                  + this->settings->get_keyboard_shortcut(KB_PLAY_CUE_POINT4_ON_DECK));
    this->help_sample_value->setText(this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER1)
                                     + "/"
                                     + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER2)
                                     + "/"
                                     + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER3)
                                     + "/"
                                     + this->settings->get_keyboard_shortcut(KB_LOAD_TRACK_ON_SAMPLER4));
    this->help_browse_value1->setText("Up/Down/Left/Right");
    this->help_browse_value2->setText(this->settings->get_keyboard_shortcut(KB_COLLAPSE_BROWSER));
}

void
Gui::can_close()
{
    // Show a pop-up asking to confirm to close.
    QMessageBox msg_box;
    msg_box.setWindowTitle("DigitalScratch");
    msg_box.setText(tr("Do you really want to quit DigitalScratch ?"));
    msg_box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msg_box.setIcon(QMessageBox::Question);
    msg_box.setDefaultButton(QMessageBox::Cancel);
    msg_box.setStyleSheet(Utils::get_current_stylesheet_css());
    if (this->nb_decks > 1)
    {
        msg_box.setWindowIcon(QIcon(ICON_2));
    }
    else
    {
        msg_box.setWindowIcon(QIcon(ICON));
    }

    // Close request confirmed.
    if (msg_box.exec() == QMessageBox::Ok)
    {
        this->window->close();
    }
}

bool
Gui::apply_main_window_style()
{
    // Apply some GUI settings manually.
    if (this->window_style == QString(GUI_STYLE_NATIVE))
    {
        // Set manually some standard icons.
        for (int i = 0; i < this->nb_samplers; i++)
        {
            this->sampler1_buttons_play[i]->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
            this->sampler2_buttons_play[i]->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
            this->sampler1_buttons_stop[i]->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
            this->sampler2_buttons_stop[i]->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
            this->sampler1_buttons_del[i]->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
            this->sampler2_buttons_del[i]->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
        }
        this->refresh_file_browser->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
        this->load_track_on_deck1_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->restart_on_deck1_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaSkipBackward));
        for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
        {
            this->cue_set_on_deck1_buttons[i]->setIcon(QIcon());
            this->cue_set_on_deck1_buttons[i]->setText("o");
            this->cue_play_on_deck1_buttons[i]->setIcon(QIcon());
            this->cue_play_on_deck1_buttons[i]->setText(">");
            this->cue_del_on_deck1_buttons[i]->setIcon(QIcon());
            this->cue_del_on_deck1_buttons[i]->setText("x");
        }
        this->load_sample1_1_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->load_sample1_2_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->load_sample1_3_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->load_sample1_4_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->load_track_on_deck2_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->restart_on_deck2_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaSkipBackward));
        for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
        {
            this->cue_set_on_deck2_buttons[i]->setIcon(QIcon());
            this->cue_set_on_deck2_buttons[i]->setText("o");
            this->cue_play_on_deck2_buttons[i]->setIcon(QIcon());
            this->cue_play_on_deck2_buttons[i]->setText("x");
            this->cue_del_on_deck2_buttons[i]->setIcon(QIcon());
            this->cue_del_on_deck2_buttons[i]->setText("x");
        }
        this->load_sample2_1_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->load_sample2_2_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->load_sample2_3_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->load_sample2_4_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
        this->show_next_key_from_deck1_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowDown));
        this->show_next_key_from_deck2_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowDown));
        this->progress_cancel_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));

        QFileIconProvider *icon_prov = this->folder_system_model->iconProvider();
        if (icon_prov != NULL)
        {
            ((TreeViewIconProvider*)icon_prov)->set_default_icons();
        }
        this->file_system_model->set_icons((QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(10, 10)),
                                           (QApplication::style()->standardIcon(QStyle::SP_DirIcon).pixmap(10, 10)));
    }
    else
    {
        // Reset some icons (can not be done nicely in CSS).
        for (int i = 0; i < this->nb_samplers; i++)
        {
            this->sampler1_buttons_play[i]->setIcon(QIcon());
            this->sampler2_buttons_play[i]->setIcon(QIcon());
            this->sampler1_buttons_stop[i]->setIcon(QIcon());
            this->sampler2_buttons_stop[i]->setIcon(QIcon());
            this->sampler1_buttons_del[i]->setIcon(QIcon());
            this->sampler2_buttons_del[i]->setIcon(QIcon());
        }
        this->refresh_file_browser->setIcon(QIcon());
        this->load_track_on_deck1_button->setIcon(QIcon());
        this->restart_on_deck1_button->setIcon(QIcon());
        for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
        {
            this->cue_set_on_deck1_buttons[i]->setIcon(QIcon());
            this->cue_set_on_deck1_buttons[i]->setText("");
            this->cue_play_on_deck1_buttons[i]->setIcon(QIcon());
            this->cue_play_on_deck1_buttons[i]->setText("");
            this->cue_del_on_deck1_buttons[i]->setIcon(QIcon());
            this->cue_del_on_deck1_buttons[i]->setText("");
        }
        this->load_sample1_1_button->setIcon(QIcon());
        this->load_sample1_2_button->setIcon(QIcon());
        this->load_sample1_3_button->setIcon(QIcon());
        this->load_sample1_4_button->setIcon(QIcon());

        if (nb_decks > 1)
        {
            this->load_track_on_deck2_button->setIcon(QIcon());
            this->restart_on_deck2_button->setIcon(QIcon());
            this->restart_on_deck1_button->setIcon(QIcon());
            for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
            {
                this->cue_set_on_deck2_buttons[i]->setIcon(QIcon());
                this->cue_set_on_deck2_buttons[i]->setText("");
                this->cue_play_on_deck2_buttons[i]->setIcon(QIcon());
                this->cue_play_on_deck2_buttons[i]->setText("");
                this->cue_del_on_deck2_buttons[i]->setIcon(QIcon());
                this->cue_del_on_deck2_buttons[i]->setText("");
            }
            this->load_sample2_1_button->setIcon(QIcon());
            this->load_sample2_2_button->setIcon(QIcon());
            this->load_sample2_3_button->setIcon(QIcon());
            this->load_sample2_4_button->setIcon(QIcon());
        }

        this->show_next_key_from_deck1_button->setIcon(QIcon());
        if (nb_decks > 1)
        {
            this->show_next_key_from_deck2_button->setIcon(QIcon());
        }
        this->progress_cancel_button->setIcon(QIcon());

        // Set icon for file browser QTreeview (can not be done nicely in CSS).
        QFileIconProvider *icon_prov = this->folder_system_model->iconProvider();
        if (icon_prov != NULL)
        {
            ((TreeViewIconProvider*)icon_prov)->set_icons(QIcon(QPixmap(PIXMAPS_PATH + this->window_style + ICON_DRIVE_SUFFIX).scaledToWidth(10,      Qt::SmoothTransformation)),
                                                          QIcon(QPixmap(PIXMAPS_PATH + this->window_style + ICON_FOLDER_SUFFIX).scaledToWidth(10,     Qt::SmoothTransformation)),
                                                          QIcon(QPixmap(PIXMAPS_PATH + this->window_style + ICON_AUDIO_FILE_SUFFIX).scaledToWidth(10, Qt::SmoothTransformation)),
                                                          QIcon(QPixmap(PIXMAPS_PATH + this->window_style + ICON_FILE_SUFFIX).scaledToWidth(10,       Qt::SmoothTransformation)));
        }
        this->file_system_model->set_icons(QPixmap(PIXMAPS_PATH + this->window_style + ICON_AUDIO_FILE_SUFFIX).scaledToWidth(10, Qt::SmoothTransformation),
                                           QPixmap(PIXMAPS_PATH + this->window_style + ICON_FOLDER_SUFFIX).scaledToWidth(10, Qt::SmoothTransformation));
    }

    // Change main window skin (using CSS).
    this->window->setStyleSheet(Utils::get_current_stylesheet_css());

    return true;
}

bool
Gui::set_folder_browser_base_path(QString in_path)
{
    // Change path of the model and set file extension filters.
    this->folder_system_model->setRootPath("");
    QStringList name_filter;
    name_filter << "*.m3u" << "*.pls" << "*.xspf";
    this->folder_system_model->setNameFilters(name_filter);
    this->folder_system_model->setNameFilterDisables(false);

    // Change root path of file browser.
    this->folder_browser->setRootIndex(this->folder_system_model->index(""));
    this->folder_browser->update();

    // Preselect the base path.
    this->folder_browser->collapseAll();
    this->folder_browser->setCurrentIndex(this->folder_system_model->index(in_path));
    this->folder_browser->setExpanded(this->folder_system_model->index(in_path), true);

    return true;
}

bool
Gui::set_file_browser_base_path(QString in_path)
{
    if (this->watcher_parse_directory->isRunning() == false)
    {
        // Hide file browser during directory analysis.
        this->file_browser->setVisible(false);

        // Stop any running file analysis.
        this->file_system_model->stop_concurrent_read_collection_from_db();
        this->file_system_model->stop_concurrent_analyse_audio_collection();

        // Show progress bar.
        this->progress_label->setText(tr("Opening ") + in_path + "...");
        this->progress_groupbox->show();
        this->progress_bar->setMinimum(0);
        this->progress_bar->setMaximum(0);

        // Set base path as title to file browser.
        this->set_file_browser_title(in_path);

        // Clear file browser.
        this->file_system_model->clear();

        // Change root path of file browser (do it in a non blocking external thread).
        QFuture<QModelIndex> future = QtConcurrent::run(this->file_system_model, &Audio_collection_model::set_root_path, in_path);
        this->watcher_parse_directory->setFuture(future);

        // Reset progress.
        this->progress_bar->reset();
    }

    return true;
}

void
Gui::run_concurrent_read_collection_from_db()
{
    // Show file browser again (file were analyse on disk).
    this->file_browser->setVisible(true);

    // Get file info from DB.
    this->file_system_model->concurrent_read_collection_from_db(); // Run in another thread.
                                                                   // Call sync_file_browser_to_audio_collection() when it's done.

    return;
}

bool
Gui::set_file_browser_playlist_tracks(Playlist *in_playlist)
{
    if (this->watcher_parse_directory->isRunning() == false)
    {
        // Hide file browser during playlist analysis.
        this->file_browser->setVisible(false);

        // Stop any running file analysis.
        this->file_system_model->stop_concurrent_read_collection_from_db();
        this->file_system_model->stop_concurrent_analyse_audio_collection();

        // Show progress bar.
        this->progress_label->setText(tr("Opening ") + in_playlist->get_name() + "...");
        this->progress_groupbox->show();
        this->progress_bar->setMinimum(0);
        this->progress_bar->setMaximum(0);

        // Set base path as title to file browser.
        this->set_file_browser_title(in_playlist->get_name());

        // Clear file browser.
        this->file_system_model->clear();

        // Set list of tracks to the file browser.
        this->file_browser->setRootIndex(QModelIndex());
        this->file_system_model->set_playlist(in_playlist);

        // Reset progress.
        this->progress_bar->reset();

        // Get file info from DB.
        this->file_system_model->concurrent_read_collection_from_db(); // Run in another thread.
                                                                       // Call sync_file_browser_to_audio_collection() when it's done.

        // Show file browser again.
        this->file_browser->setVisible(true);
    }

    return true;
}

void
Gui::sync_file_browser_to_audio_collection()
{
    // Reset file browser root node to audio collection model's root.
    this->file_browser->update();
    this->file_browser->setRootIndex(this->file_system_model->get_root_index());
    this->resize_file_browser_columns();

    // Hide progress bar.
    this->progress_label->setText("");
    this->progress_groupbox->hide();
}

bool
Gui::set_file_browser_title(QString in_title)
{
    // Change file browser title (which contains base dir for tracks).
    this->file_browser_gbox->setTitle(tr("File browser") + " [" + in_title + "]");

    return true;
}

void
Gui::run_sampler_decoding_process(unsigned short int in_deck_index,
                                  unsigned short int in_sampler_index)
{
    // Select deck.
    this->highlight_deck_sampler_area(in_deck_index);

    // Get selected file path.
    Audio_collection_item *item = static_cast<Audio_collection_item*>((this->file_browser->currentIndex()).internalPointer());
    QFileInfo info(item->get_full_path());
    if (info.isFile() == true)
    {
        // Execute decoding.
        QList<QSharedPointer<Audio_file_decoding_process>> samplers;
        if (in_deck_index == 0)
        {
            samplers = this->dec_samplers[0];
        }
        else
        {
            samplers = this->dec_samplers[1];
        }
        if (samplers[in_sampler_index]->run(info.absoluteFilePath(), "", "") == false)
        {
            qCWarning(DS_FILE) << "can not decode " << info.absoluteFilePath();
        }
        else
        {
            this->playback->reset_sampler(in_deck_index, in_sampler_index);
            this->set_sampler_state(in_deck_index, in_sampler_index, false);
            this->playback->set_sampler_state(in_deck_index, in_sampler_index, false);
        }
    }

    return;
}

void
Gui::select_and_run_sample1_decoding_process_deck1()
{
    // Check the button.
    this->load_sample1_1_button->setEnabled(false);
    this->load_sample1_1_button->setChecked(true);

    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Decode and play sample audio file on sampler 1.
    this->run_sample_1_decoding_process();

    // Release the button.
    this->load_sample1_1_button->setEnabled(true);
    this->load_sample1_1_button->setChecked(false);
}

void
Gui::select_and_run_sample1_decoding_process_deck2()
{
    // Check the button.
    this->load_sample2_1_button->setEnabled(false);
    this->load_sample2_1_button->setChecked(true);

    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Decode and play sample audio file on sampler 1.
    this->run_sample_1_decoding_process();

    // Release the button.
    this->load_sample2_1_button->setEnabled(true);
    this->load_sample2_1_button->setChecked(false);
}

void
Gui::select_and_run_sample2_decoding_process_deck1()
{
    // Check the button.
    this->load_sample1_2_button->setEnabled(false);
    this->load_sample1_2_button->setChecked(true);

    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Decode and play sample audio file on sampler 2.
    this->run_sample_2_decoding_process();

    // Release the button.
    this->load_sample1_2_button->setEnabled(true);
    this->load_sample1_2_button->setChecked(false);
}

void
Gui::select_and_run_sample2_decoding_process_deck2()
{
    // Check the button.
    this->load_sample2_2_button->setEnabled(false);
    this->load_sample2_2_button->setChecked(true);

    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Decode and play sample audio file on sampler 2.
    this->run_sample_2_decoding_process();

    // Release the button.
    this->load_sample2_2_button->setEnabled(true);
    this->load_sample2_2_button->setChecked(false);
}

void
Gui::select_and_run_sample3_decoding_process_deck1()
{
    // Check the button.
    this->load_sample1_3_button->setEnabled(false);
    this->load_sample1_3_button->setChecked(true);

    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Decode and play sample audio file on sampler 3.
    this->run_sample_3_decoding_process();

    // Release the button.
    this->load_sample1_3_button->setEnabled(true);
    this->load_sample1_3_button->setChecked(false);
}

void
Gui::select_and_run_sample3_decoding_process_deck2()
{
    // Check the button.
    this->load_sample2_3_button->setEnabled(false);
    this->load_sample2_3_button->setChecked(true);

    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Decode and play sample audio file on sampler 3.
    this->run_sample_3_decoding_process();

    // Release the button.
    this->load_sample2_3_button->setEnabled(true);
    this->load_sample2_3_button->setChecked(false);
}

void
Gui::select_and_run_sample4_decoding_process_deck1()
{
    // Check the button.
    this->load_sample1_4_button->setEnabled(false);
    this->load_sample1_4_button->setChecked(true);

    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Decode and play sample audio file on sampler 4.
    this->run_sample_4_decoding_process();

    // Release the button.
    this->load_sample1_4_button->setEnabled(true);
    this->load_sample1_4_button->setChecked(false);
}

void
Gui::select_and_run_sample4_decoding_process_deck2()
{
    // Check the button.
    this->load_sample2_4_button->setEnabled(false);
    this->load_sample2_4_button->setChecked(true);

    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Decode and play sample audio file on sampler 4.
    this->run_sample_4_decoding_process();

    // Release the button.
    this->load_sample2_4_button->setEnabled(true);
    this->load_sample2_4_button->setChecked(false);
}

void
Gui::run_sample_1_decoding_process()
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {
        this->run_sampler_decoding_process(1, 0);
    }
    else
    {
        this->run_sampler_decoding_process(0, 0);
    }

    return;
}

void
Gui::run_sample_2_decoding_process()
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {
        this->run_sampler_decoding_process(1, 1);
    }
    else
    {
        this->run_sampler_decoding_process(0, 1);
    }

    return;
}

void
Gui::run_sample_3_decoding_process()
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {
        this->run_sampler_decoding_process(1, 2);
    }
    else
    {
        this->run_sampler_decoding_process(0, 2);
    }

    return;
}

void
Gui::run_sample_4_decoding_process()
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {
        this->run_sampler_decoding_process(1, 3);
    }
    else
    {
        this->run_sampler_decoding_process(0, 3);
    }

    return;
}

void
Gui::set_sampler_1_text(QString in_text, unsigned short int in_sampler_index)
{
    this->sampler1_trackname[in_sampler_index]->setText(in_text);
    return;
}

void
Gui::set_sampler_2_text(QString in_text, unsigned short int in_sampler_index)
{
    this->sampler2_trackname[in_sampler_index]->setText(in_text);
    return;
}

void
Gui::on_sampler_button_play_click(unsigned short int in_deck_index,
                                  unsigned short int in_sampler_index)
{
    // First stop playback (and return to beginning of the song).
    this->set_sampler_state(in_deck_index, in_sampler_index, false);
    this->playback->set_sampler_state(in_deck_index, in_sampler_index, false);

    // Then start playback again.
    this->set_sampler_state(in_deck_index, in_sampler_index, true);
    this->playback->set_sampler_state(in_deck_index, in_sampler_index, true);

    // Select playback area (if not already done).
    this->highlight_deck_sampler_area(in_deck_index);

    return;
}

void
Gui::on_sampler_button_stop_click(unsigned short int in_deck_index,
                                  unsigned short int in_sampler_index)
{
    // Stop playback (and return to beginning of the song).
    this->set_sampler_state(in_deck_index, in_sampler_index, false);
    this->playback->set_sampler_state(in_deck_index, in_sampler_index, false);

    // Select playback area (if not already done).
    this->highlight_deck_sampler_area(in_deck_index);

    return;
}

void
Gui::on_sampler_button_del_click(unsigned short int in_deck_index,
                                 unsigned short int in_sampler_index)
{
    // Remove track loaded in the sampler.
    this->playback->del_sampler(in_deck_index, in_sampler_index);
    this->set_sampler_state(in_deck_index, in_sampler_index, false);

    // Select playback area (if not already done).
    this->highlight_deck_sampler_area(in_deck_index);

    return;
}

void
Gui::on_progress_cancel_button_click()
{
    // Stop any running file analysis.
    this->file_system_model->stop_concurrent_read_collection_from_db();
    this->file_system_model->stop_concurrent_analyse_audio_collection();
}

void
Gui::on_file_browser_expand(QModelIndex)
{
    this->resize_file_browser_columns();
}

void
Gui::on_file_browser_header_click(int in_index)
{    
    // Get the order.
    Qt::SortOrder order = this->file_browser->header()->sortIndicatorOrder();

    // Sort the items.
    this->file_browser->sortByColumn(in_index, order);
}

void
Gui::on_file_browser_double_click(QModelIndex in_model_index)
{
    if (this->watcher_parse_directory->isRunning() == false)
    {
        // Get path (file for a playlist, or just a directory).
        QString path = this->folder_system_model->filePath(in_model_index);

        QFileInfo file_info(path);
        if (file_info.isFile() == true)
        {
            Playlist             *playlist         = new Playlist(file_info.absolutePath(), file_info.baseName());
            Playlist_persistence *playlist_persist = new Playlist_persistence();

            // It is a m3u playlist, parse it and show track list in file browser.
            if (file_info.suffix().compare(QString("m3u"), Qt::CaseInsensitive) == 0)
            {
                // Open M3U playlist
                if (playlist_persist->read_m3u(path, playlist) == true)
                {
                    // Populate file browser.
                    this->set_file_browser_playlist_tracks(playlist);
                }
                else
                {
                    qCWarning(DS_FILE) << "can not open m3u playlist " << qPrintable(path);
                }
            }
            else if (file_info.suffix().compare(QString("pls"), Qt::CaseInsensitive) == 0)
            {
                // Open PLS playlist
                if (playlist_persist->read_pls(path, playlist) == true)
                {
                    // Populate file browser.
                    this->set_file_browser_playlist_tracks(playlist);
                }
                else
                {
                    qCWarning(DS_FILE) << "can not open pls playlist " << qPrintable(path);
                }
            }

            // Cleanup.
            delete playlist;
            delete playlist_persist;
        }
        else
        {
            // It is a directory, open selected directory in file browser.
            this->set_file_browser_base_path(path);
        }
    }
}

void
Gui::resize_file_browser_columns()
{
    this->file_browser->resizeColumnToContents(COLUMN_KEY);
    this->file_browser->resizeColumnToContents(COLUMN_FILE_NAME);
}

void
Gui::select_and_run_audio_file_decoding_process_deck1()
{
    // Check the button.
    this->load_track_on_deck1_button->setEnabled(false);
    this->load_track_on_deck1_button->setChecked(true);

    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Decode and play selected audio file on deck 1.
    this->run_audio_file_decoding_process();

    // Release the button.
    this->load_track_on_deck1_button->setEnabled(true);
    this->load_track_on_deck1_button->setChecked(false);
}

void
Gui::select_and_run_audio_file_decoding_process_deck2()
{
    // Check the button.
    this->load_track_on_deck2_button->setEnabled(false);
    this->load_track_on_deck2_button->setChecked(true);

    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Decode and play selected audio file on deck 2.
    this->run_audio_file_decoding_process();

    // Release the button.
    this->load_track_on_deck2_button->setEnabled(true);
    this->load_track_on_deck2_button->setChecked(false);
}

void
Gui::run_audio_file_decoding_process()
{
    // Force processing events to refresh main window before running decoding.
    QApplication::processEvents();

    // Get selected file path.
    Audio_collection_item *item = static_cast<Audio_collection_item*>((this->file_browser->currentIndex()).internalPointer());
    QFileInfo info(item->get_full_path());
    if (info.isFile() == true)
    {
        // Get selected deck/sampler.
        unsigned short int deck_index = 0;
        QLabel    *deck_track_name = this->deck1_track_name;
        QLabel   **deck_cue_point  = this->cue_point_deck1_labels;
        Waveform  *deck_waveform   = this->deck1_waveform;
        QSharedPointer<Audio_file_decoding_process> decode_process = this->decs[0];
        if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
        {
            deck_index = 1;
            deck_track_name = this->deck2_track_name;
            deck_cue_point  = this->cue_point_deck2_labels;
            deck_waveform   = this->deck2_waveform;
            decode_process  = this->decs[1];
        }

        // Execute decoding if not trying to open the existing track.
        if (info.fileName().compare(deck_track_name->text()) != 0 )
        {
            // Stop playback.
            this->playback->stop(deck_index);

            // Clear audio track and waveform.
            decode_process->clear();
            deck_waveform->reset();
            deck_waveform->update();

            // Decode track.
            if (decode_process->run(info.absoluteFilePath(), item->get_file_hash(), item->get_data(COLUMN_KEY).toString()) == false)
            {
                qCWarning(DS_FILE) << "can not decode " << info.absoluteFilePath();
            }
        }

        // Force waveform computation.
        deck_waveform->reset();

        // Reset playback process.
        this->playback->reset(deck_index);
        deck_waveform->move_slider(0.0);

        // Reset cue point.
        for (unsigned short int i = 0; i < MAX_NB_CUE_POINTS; i++)
        {
            deck_waveform->move_cue_slider(i, this->playback->get_cue_point(deck_index, i));
            deck_cue_point[i]->setText(this->playback->get_cue_point_str(deck_index, i));
        }

        // Update waveform.
        deck_waveform->update();
    }

    return;
}

void
Gui::set_deck1_key(const QString &in_key)
{
    if (in_key.isEmpty() == false)
    {
        this->deck1_key->show();
        this->deck1_key->setText("♪ " + in_key);
    }
    else
    {
        this->deck1_key->hide();
        this->deck1_key->setText("");
    }
}

void
Gui::set_deck2_key(const QString &in_key)
{
    if (in_key.isEmpty() == false)
    {
        this->deck2_key->show();
        this->deck2_key->setText("♪ " + in_key);
    }
    else
    {
        this->deck2_key->hide();
        this->deck2_key->setText("");
    }
}

void
Gui::set_remaining_time(unsigned int in_remaining_time, int in_deck_index)
{
    // Split remaining time (which is in msec) into minutes, seconds and milliseconds.
    int remaining_time_by_1000 = in_remaining_time / 1000.0;
    div_t tmp_division;
    tmp_division = div(remaining_time_by_1000, 60);
    QString min  = QString::number(tmp_division.quot);
    QString sec  = QString::number(tmp_division.rem);
    QString msec = QString::number(in_remaining_time).right(3);

    // Change displayed remaining time.
    if (min.compare(this->decks_remaining_time[in_deck_index]->min->text()) != 0)
    {
        if (min.size() == 1)
        {
            this->decks_remaining_time[in_deck_index]->min->setText("0" + min);
        }
        else
        {
            this->decks_remaining_time[in_deck_index]->min->setText(min);
        }
    }
    if (sec.compare(this->decks_remaining_time[in_deck_index]->sec->text()) != 0)
    {
        if (sec.size() == 1)
        {
            this->decks_remaining_time[in_deck_index]->sec->setText("0" + sec);
        }
        else
        {
            this->decks_remaining_time[in_deck_index]->sec->setText(sec);
        }
    }
    if (msec.compare(this->decks_remaining_time[in_deck_index]->msec->text()) != 0)
    {
        this->decks_remaining_time[in_deck_index]->msec->setText(msec);
    }

    // Move slider on waveform when remaining time changed.
    if (in_deck_index == 0)
    {

       this->deck1_waveform->move_slider(this->playback->get_position(0));
    }
    else
    {
        this->deck2_waveform->move_slider(this->playback->get_position(1));
    }

    return;
}

void
Gui::set_sampler_remaining_time(unsigned int in_remaining_time,
                                int          in_deck_index,
                                int          in_sampler_index)
{
    if (in_remaining_time == 0)
    {
        if (in_deck_index == 0)
        {
            this->sampler1_remainingtime[in_sampler_index]->setText("- 00");
        }
        else
        {
            this->sampler2_remainingtime[in_sampler_index]->setText("- 00");
        }
    }
    else
    {
        // Split remaining time (which is in msec) into minutes, seconds and milliseconds.
        int remaining_time_by_1000 = in_remaining_time / 1000.0;
        div_t tmp_division;
        tmp_division = div(remaining_time_by_1000, 60);
        QString sec  = QString::number(tmp_division.rem);

        // Change displayed remaining time (if different than previous one).
        if (in_deck_index == 0 &&
            sec.compare(this->sampler1_remainingtime[in_sampler_index]->text()) != 0)
        {
            if (sec.size() == 1)
            {
                this->sampler1_remainingtime[in_sampler_index]->setText("- 0" + sec);
            }
            else
            {
                this->sampler1_remainingtime[in_sampler_index]->setText("- " + sec);
            }
        }
        if (in_deck_index == 1 &&
            sec.compare(this->sampler2_remainingtime[in_sampler_index]->text()) != 0)
        {
            if (sec.size() == 1)
            {
                this->sampler2_remainingtime[in_sampler_index]->setText("- 0" + sec);
            }
            else
            {
                this->sampler2_remainingtime[in_sampler_index]->setText("- " + sec);
            }
        }
    }

    return;
}

void
Gui::set_sampler_state(int  in_deck_index,
                       int  in_sampler_index,
                       bool in_state)
{
    // Change state only if a sample is loaded and playing.
    if ((this->playback->is_sampler_loaded(in_deck_index, in_sampler_index) == true) && (in_state == true))
    {
        if (in_deck_index == 0)
        {
            this->sampler1_buttons_play[in_sampler_index]->setChecked(true);
            this->sampler1_buttons_stop[in_sampler_index]->setChecked(false);
        }
        else
        {
            this->sampler2_buttons_play[in_sampler_index]->setChecked(true);
            this->sampler2_buttons_stop[in_sampler_index]->setChecked(false);
        }

    }
    else // Sampler is stopping or is not loaded, make play button inactive.
    {
        if (in_deck_index == 0)
        {
            this->sampler1_buttons_play[in_sampler_index]->setChecked(false);
            this->sampler1_buttons_stop[in_sampler_index]->setChecked(true);
        }
        else
        {
            this->sampler2_buttons_play[in_sampler_index]->setChecked(false);
            this->sampler2_buttons_stop[in_sampler_index]->setChecked(true);
        }
    }

    return;
}

void
Gui::switch_speed_mode(bool in_mode, int in_deck_index)
{
    Q_UNUSED(in_deck_index); // FIXME: modify method to handle 2 decks.

    // Mode: false=manual, true=timecode
    if (in_mode == true)
    {
        this->deck1_timecode_manual_button->setText(tr("TIMECODE"));
        this->speed_up_on_deck1_button->hide();
        this->speed_down_on_deck1_button->hide();
        this->accel_up_on_deck1_button->hide();
        this->accel_down_on_deck1_button->hide();
    }
    else
    {
        this->deck1_timecode_manual_button->setText(tr("MANUAL"));
        this->speed_up_on_deck1_button->show();
        this->speed_down_on_deck1_button->show();
        this->accel_up_on_deck1_button->show();
        this->accel_down_on_deck1_button->show();
    }
}

void
Gui::update_speed_label(float in_speed, int in_deck_index)
{
    double percent = (double)(floorf((in_speed * 100.0) * 10.0) / 10.0);
    QString sp = QString("%1%2").arg(percent < 0 ? '-' : '+').arg(qAbs(percent), 5, 'f', 1, '0') + '%';

    if (in_deck_index == 0)
    {
        this->deck1_speed->setText(sp);
    }
    else
    {
        this->deck2_speed->setText(sp);
    }
}

void
Gui::speed_up_down(float in_speed_inc, int in_deck_index)
{
    // Select deck.
    this->highlight_deck_sampler_area(in_deck_index);

    // Increment speed.
    if (in_deck_index == 0)
    {
        this->params[0]->inc_speed(in_speed_inc);
    }
    else
    {
        this->params[1]->inc_speed(in_speed_inc);
    }
}

void
Gui::deck1_jump_to_position(float in_position)
{
    this->playback->jump_to_position(0, in_position);
    this->highlight_deck_sampler_area(0);

    return;
}

void
Gui::deck2_jump_to_position(float in_position)
{
    this->playback->jump_to_position(1, in_position);
    this->highlight_deck_sampler_area(1);

    return;
}

void
Gui::deck_go_to_begin()
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {
        // Deck 2.
        this->playback->jump_to_position(1, 0.0);
    }
    else
    {
        // Deck 1.
        this->playback->jump_to_position(0, 0.0);
    }

    return;
}

void
Gui::deck1_go_to_begin()
{
    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Jump.
    this->deck_go_to_begin();

    return;
}

void
Gui::deck2_go_to_begin()
{
    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Jump.
    this->deck_go_to_begin();

    return;
}

void
Gui::deck_set_cue_point(int in_cue_point_number)
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {       
        // Deck 2.
        this->deck2_waveform->move_cue_slider(in_cue_point_number, this->playback->get_position(1));
        this->playback->store_cue_point(1, in_cue_point_number);
        this->cue_point_deck2_labels[in_cue_point_number]->setText(this->playback->get_cue_point_str(1, in_cue_point_number));
    }
    else
    {
        // Deck 1.
        this->deck1_waveform->move_cue_slider(in_cue_point_number, this->playback->get_position(0));
        this->playback->store_cue_point(0, in_cue_point_number);
        this->cue_point_deck1_labels[in_cue_point_number]->setText(this->playback->get_cue_point_str(0, in_cue_point_number));
    }

    return;
}

void
Gui::deck1_set_cue_point(int in_cue_point_number)
{
    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Set cue point.
    this->deck_set_cue_point(in_cue_point_number);

    return;
}

void
Gui::deck2_set_cue_point(int in_cue_point_number)
{
    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Set cue point.
    this->deck_set_cue_point(in_cue_point_number);

    return;
}

void
Gui::deck_go_to_cue_point(int in_cue_point_number)
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {        
        // Deck 2.
        this->playback->jump_to_cue_point(1, in_cue_point_number);
    }
    else
    {
        // Deck 1.
        this->playback->jump_to_cue_point(0, in_cue_point_number);
    }

    return;
}

void
Gui::deck1_go_to_cue_point(int in_cue_point_number)
{
    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Jump.
    this->deck_go_to_cue_point(in_cue_point_number);

    return;
}

void
Gui::deck2_go_to_cue_point(int in_cue_point_number)
{
    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Jump.
    this->deck_go_to_cue_point(in_cue_point_number);

    return;
}

void
Gui::deck_del_cue_point(int in_cue_point_number)
{
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {
        // Deck 2.
        this->playback->delete_cue_point(1, in_cue_point_number);
        this->deck2_waveform->move_cue_slider(in_cue_point_number, 0.0);
        this->cue_point_deck2_labels[in_cue_point_number]->setText(this->playback->get_cue_point_str(1, in_cue_point_number));
    }
    else
    {
        // Deck 1.
        this->playback->delete_cue_point(0, in_cue_point_number);
        this->deck1_waveform->move_cue_slider(in_cue_point_number, 0.0);
        this->cue_point_deck1_labels[in_cue_point_number]->setText(this->playback->get_cue_point_str(0, in_cue_point_number));
    }

    return;
}

void
Gui::deck1_del_cue_point(int in_cue_point_number)
{
    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Delete point.
    this->deck_del_cue_point(in_cue_point_number);

    return;
}

void
Gui::deck2_del_cue_point(int in_cue_point_number)
{
    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Delete point.
    this->deck_del_cue_point(in_cue_point_number);

    return;
}

void
Gui::switch_playback_selection()
{
    // Switch deck selection.
    if (this->nb_decks > 1)
    {
        if (this->deck1_gbox->property("selected").toBool() == true)
        {
            // Select deck/sample #2
            this->highlight_deck_sampler_area(1);
        }
        else
        {
            // Select deck/sample #1
            this->highlight_deck_sampler_area(0);
        }
    }

    return;
}

void
Gui::select_playback_1()
{
    // Select deck/sample #1
    this->highlight_deck_sampler_area(0);

    return;
}

void
Gui::select_playback_2()
{
    if (this->nb_decks > 1)
    {
        // Select deck/sample #2
        this->highlight_deck_sampler_area(1);
    }

    return;
}

void
Gui::highlight_deck_sampler_area(unsigned short int in_deck_index)
{
    bool switch_on = false;
    if (in_deck_index == 0)
    {
        switch_on = true;
    }

    // Select correct pair deck+sampler.
    this->deck1_gbox->setProperty("selected", switch_on);
    this->deck2_gbox->setProperty("selected", !switch_on);
    this->sampler1_gbox->setProperty("selected", switch_on);
    this->sampler2_gbox->setProperty("selected", !switch_on);

    // Redraw widget (necessary to reparse stylesheet).
    this->deck1_gbox->redraw();
    this->deck2_gbox->redraw();
    this->sampler1_gbox->redraw();
    this->sampler2_gbox->redraw();

    return;
}

void
Gui::hover_playback(int in_deck_index)
{
    this->highlight_border_deck_sampler_area(in_deck_index, true);

    return;
}

void
Gui::unhover_playback(int in_deck_index)
{
    this->highlight_border_deck_sampler_area(in_deck_index, false);

    return;
}

void
Gui::highlight_border_deck_sampler_area(unsigned short int in_deck_index,
                                        bool               switch_on)
{
    if (in_deck_index == 0)
    {
        // highlight pair deck+sampler.
        this->deck1_gbox->setProperty("hover", switch_on);
        this->sampler1_gbox->setProperty("hover", switch_on);

        // Redraw widget (necessary to reparse stylesheet).
        this->deck1_gbox->redraw();
        this->sampler1_gbox->redraw();
    }

    if (in_deck_index == 1)
    {
        // highlight pair deck+sampler.
        this->deck2_gbox->setProperty("hover", switch_on);
        this->sampler2_gbox->setProperty("hover", switch_on);

        // Redraw widget (necessary to reparse stylesheet).
        this->deck2_gbox->redraw();
        this->sampler2_gbox->redraw();
    }

    return;
}

void
Gui::select_and_show_next_keys_deck1()
{
    // Check the button.
    this->show_next_key_from_deck1_button->setEnabled(false);
    this->show_next_key_from_deck1_button->setChecked(true);

    // Select deck 1.
    this->highlight_deck_sampler_area(0);

    // Show next keys from deck 1.
    this->show_next_keys();

    // Release the button.
    this->show_next_key_from_deck1_button->setEnabled(true);
    this->show_next_key_from_deck1_button->setChecked(false);
}

void
Gui::select_and_show_next_keys_deck2()
{
    // Check the button.
    this->show_next_key_from_deck2_button->setEnabled(false);
    this->show_next_key_from_deck2_button->setChecked(true);

    // Select deck 2.
    this->highlight_deck_sampler_area(1);

    // Show next keys from deck 2.
    this->show_next_keys();

    // Release the button.
    this->show_next_key_from_deck2_button->setEnabled(true);
    this->show_next_key_from_deck2_button->setChecked(false);
}

void
Gui::show_next_keys()
{
    // Get music key of selected deck/sampler.
    QString deck_key = this->ats[0]->get_music_key();
    if ((this->nb_decks > 1) && (this->deck2_gbox->is_selected() == true))
    {
        deck_key = this->ats[1]->get_music_key();
    }

    // Get next and prev keys and iterate over the full audio collection for highlighting.
    if (deck_key.length() > 0)
    {
        // Collapse all tree.
        this->file_browser->collapseAll();

        // Get next keys and highlight.
        QString next_key;
        QString prev_key;
        QString oppos_key;
        Utils::get_next_music_keys(deck_key, next_key, prev_key, oppos_key);
        QList<QModelIndex> directories = this->file_system_model->set_next_keys(next_key, prev_key, oppos_key);

        // Expand directories containing file of next keys.
        foreach (QModelIndex index, directories)
        {
            this->file_browser->expand(index);
        }
    }

    return;
}

PlaybackQGroupBox::PlaybackQGroupBox(const QString &title) : QGroupBox(title)
{
    // Init.
    this->l_selected = false;
    this->setAcceptDrops(true);

    return;
}

PlaybackQGroupBox::~PlaybackQGroupBox()
{
    return;
}

void
PlaybackQGroupBox::redraw()
{
    this->style()->unpolish(this);
    this->style()->polish(this);

    return;
}

void
PlaybackQGroupBox::mousePressEvent(QMouseEvent *in_mouse_event)
{
    Q_UNUSED(in_mouse_event);
    emit this->selected();

    return;
}

void
PlaybackQGroupBox::enterEvent(QEvent *in_event)
{
    Q_UNUSED(in_event);
    emit this->hover();

    return;
}

void
PlaybackQGroupBox::leaveEvent(QEvent *in_event)
{
    Q_UNUSED(in_event);
    emit this->unhover();

    return;
}

void
PlaybackQGroupBox::dragEnterEvent(QDragEnterEvent *in_event)
{
    // Only accept plain text to drop in.
    if (in_event->mimeData()->hasFormat("application/vnd.text.list"))
    {
        in_event->acceptProposedAction();
    }
}

void
PlaybackQGroupBox::dropEvent(QDropEvent *in_event)
{
    // Decode dragged file names as a string list.
    QByteArray encoded_dragged_data = in_event->mimeData()->data("application/vnd.text.list");
    QDataStream encoded_dragged_stream(&encoded_dragged_data, QIODevice::ReadOnly);
    QStringList new_items;
    int rows = 0;
    while (encoded_dragged_stream.atEnd() == false)
    {
        QString text;
        encoded_dragged_stream >> text;
        new_items << text;
        rows++;
    }

    // Accept the drop action.
    in_event->acceptProposedAction();

    // Send a signal saying that a file was dropped into the groupbox.
    emit file_dropped();
}

SpeedQPushButton::SpeedQPushButton(const QString &title) : QPushButton(title)
{
    // Init.
    this->setProperty("right_clicked", false);
    this->setProperty("pressed", false);

    return;
}

SpeedQPushButton::~SpeedQPushButton()
{
    return;
}

void
SpeedQPushButton::redraw()
{
    this->style()->unpolish(this);
    this->style()->polish(this);

    return;
}

void
SpeedQPushButton::mousePressEvent(QMouseEvent *in_mouse_event)
{
    // Force state "pressed" in style sheet even it is a right click event.
    this->setProperty("pressed", true);
    this->redraw();

    QPushButton::mousePressEvent(in_mouse_event);

    return;
}

void
SpeedQPushButton::mouseReleaseEvent(QMouseEvent *in_mouse_event)
{
    // Unpress the button.
    this->setProperty("pressed", false);
    this->redraw();

    QPushButton::mouseReleaseEvent(in_mouse_event);

    // Forward the right click event.
    if (in_mouse_event->button() == Qt::RightButton)
    {
        emit this->right_clicked();
    }

    return;
}

QSamplerContainerWidget::QSamplerContainerWidget(unsigned short  in_deck_index,
                                                 unsigned short  in_sampler_index,
                                                 Gui            *in_dropto_object) : QWidget()
{
    this->setAcceptDrops(true);
    this->deck_index    = in_deck_index;
    this->sampler_index = in_sampler_index;

    // Connect drop actions (load file in sampler).
    QObject::connect(this,             SIGNAL(file_dropped_in_sampler(unsigned short int, unsigned short int)),
                     in_dropto_object, SLOT(run_sampler_decoding_process(unsigned short int, unsigned short int)));
    return;
}

QSamplerContainerWidget::~QSamplerContainerWidget()
{
    return;
}

void
QSamplerContainerWidget::dragEnterEvent(QDragEnterEvent *in_event)
{
    // Only accept plain text to drop in.
    if (in_event->mimeData()->hasFormat("application/vnd.text.list"))
    {
        in_event->acceptProposedAction();
    }
}

void
QSamplerContainerWidget::dropEvent(QDropEvent *in_event)
{
    // Accept the drop action.
    in_event->acceptProposedAction();

    // Send a signal saying that a file was dropped into the sampler.
    emit file_dropped_in_sampler(this->deck_index, this->sampler_index);
}

TreeViewIconProvider::TreeViewIconProvider()
{
    this->set_default_icons();
}

TreeViewIconProvider::~TreeViewIconProvider()
{
}

void TreeViewIconProvider::set_icons(const QIcon &drive,
                                     const QIcon &folder,
                                     const QIcon &file,
                                     const QIcon &other)
{
    this->drive  = drive;
    this->folder = folder;
    this->file   = file;
    this->other  = other;
}

void TreeViewIconProvider::set_default_icons()
{
    this->drive  = QIcon(QApplication::style()->standardIcon(QStyle::SP_DriveHDIcon));
    this->folder = QIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    this->file   = QIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    this->other  = QIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
}

QIcon TreeViewIconProvider::icon(IconType type) const
{
    if (type == TreeViewIconProvider::Drive)
    {
        return this->drive;
    }
    else if (type == TreeViewIconProvider::Folder)
    {
        return this->folder;
    }
    else if (type == TreeViewIconProvider::File)
    {
        return this->file;
    }

    return this->other;
}

QIcon TreeViewIconProvider::icon(const QFileInfo &info) const
{
    if (info.isRoot() == true)
    {
        return this->drive;
    }
    else if (info.isDir() == true)
    {
        return this->folder;
    }
    else if (info.isFile() == true)
    {
        return this->file;
    }

    return this->other;
}

QString TreeViewIconProvider::type(const QFileInfo &info) const
{
    if (info.isRoot() == true)
    {
        return QString("Drive");
    }
    else if (info.isDir() == true)
    {
        return QString("Folder");
    }
    else if (info.isFile() == true)
    {
        return QString("File");
    }

    return QString("File");
}
