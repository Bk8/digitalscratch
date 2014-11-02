/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch Player                           */
/*                                                                            */
/*                                                                            */
/*---------------------------------------------------------------( main.cpp )-*/
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
/*           DigitalScratch player application starting point                 */
/*                                                                            */
/*============================================================================*/

#include <QtDebug>
#include <QApplication>
#include <QTextCodec>
#include <QProcess>
#include <QThreadPool>

#include "application_logging.h"
#include "gui.h"
#include "audio_track.h"
#include "audio_file_decoding_process.h"
#include "audio_track_playback_process.h"
#include "sound_driver_access_rules.h"
#include "jack_access_rules.h"
#include "playback_parameters.h"
#include "timecode_control_process.h"
#include "sound_capture_and_playback_process.h"
#include "singleton.h"
#include "application_const.h"

int main(int argc, char *argv[])
{
    // Logging settings.
    qSetMessagePattern("[%{type}] | %{category} | %{function}@%{line} | %{message}");
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n \
                                                    ds.file.debug=true\n \
                                                    *.warning=true\n \
                                                    *.critical=true\n"));


    // Number of samplers. FIXME: move to application_const.h ?
    unsigned short int nb_samplers = 4;

    // Create application.
    QApplication app(argc, argv);

    // Set max number of simultaneous new threads.
#ifdef WIN32
    QThreadPool::globalInstance()->setMaxThreadCount(1);
#else
    if (QThreadPool::globalInstance()->maxThreadCount() > 1)
    {
        QThreadPool::globalInstance()->setMaxThreadCount(QThreadPool::globalInstance()->maxThreadCount() - 1);
    }
#endif

    // Application settings management.
    Application_settings *settings = &Singleton<Application_settings>::get_instance();

    // Execute user's defined program at startup.
    if (settings->get_extern_prog() != "")
    {
        QProcess *process = new QProcess();
        process->start(settings->get_extern_prog());
    }

    // Create tracks, sampler, decoder process,... for each deck.
    QList<QSharedPointer<Audio_track>>                        ats;
    QList<QSharedPointer<Audio_file_decoding_process>>        dec_procs;
    QList<QList<QSharedPointer<Audio_track>>>                 at_samplers;
    QList<QList<QSharedPointer<Audio_file_decoding_process>>> dec_sampler_procs;
    QList<QSharedPointer<Playback_parameters>>                play_params;
    for (auto i = 1; i <= settings->get_nb_decks(); i++)
    {
        // Track for a deck.
        QSharedPointer<Audio_track>                 at(new Audio_track(MAX_MINUTES_TRACK, settings->get_sample_rate()));
        QSharedPointer<Audio_file_decoding_process> dec_proc(new Audio_file_decoding_process(at.data()));
        ats << at;
        dec_procs << dec_proc;

        // Playback parameters.
        QSharedPointer<Playback_parameters> play_param(new Playback_parameters);
        play_params << play_param;

        // Set of samplers for a deck.
        QList<QSharedPointer<Audio_track>>                 at_sampler;
        QList<QSharedPointer<Audio_file_decoding_process>> dec_sampler_proc;
        for (auto j = 1; j <= nb_samplers; j++)
        {
            QSharedPointer<Audio_track>                 at_s(new Audio_track(MAX_MINUTES_SAMPLER, settings->get_sample_rate()));
            QSharedPointer<Audio_file_decoding_process> dec_s_proc(new Audio_file_decoding_process(at_s.data()));
            at_sampler << at_s;
            dec_sampler_proc << dec_s_proc;
        }
        at_samplers << at_sampler;
        dec_sampler_procs << dec_sampler_proc;
    }

    // Process which analyze captured timecode data.
    Timecode_control_process *tcode_control = new Timecode_control_process(play_params,
                                                                           settings->get_nb_decks(),
                                                                           settings->get_vinyl_type(),
                                                                           settings->get_sample_rate());
    int *dscratch_ids;
    dscratch_ids = new int[settings->get_nb_decks()];
    for (auto i = 0; i < settings->get_nb_decks(); i++)
    {
        dscratch_ids[i] = tcode_control->get_dscratch_id(i);
    }

    // Playback process.
    Audio_track_playback_process *at_playback = new Audio_track_playback_process(ats,
                                                                                 at_samplers,
                                                                                 play_params,
                                                                                 settings->get_nb_decks(),
                                                                                 nb_samplers);

    // Access sound card.
    // TODO add settings to check if we want to use the timecode to get playback params or not (so no capture).
    Sound_driver_access_rules *sound_card = new Jack_access_rules(settings->get_nb_decks() * 2);
    sound_card->set_capture(true);

    // Sound capture and playback process.
    Sound_capture_and_playback_process *capture_and_playback = new Sound_capture_and_playback_process(tcode_control,
                                                                                                      at_playback,
                                                                                                      sound_card);
    // TODO if not using timecode to get playback params
    //Only_playback_process *playback_external_params = new Playback_process_with_external_parameters(playback, sound_card);

    // Create GUI.
    Gui *gui = new Gui(ats,
                       at_samplers,
                       dec_procs,
                       dec_sampler_procs,
                       play_params,
                       at_playback,
                       settings->get_nb_decks(),
                       sound_card,
                       capture_and_playback,
                       dscratch_ids);

    // Forward the quit call.
    app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));

    // Start application.
    app.exec();

    // Cleanup.
    delete gui;
    delete sound_card;
    delete capture_and_playback;
    delete tcode_control;
    delete at_playback;
    delete[] dscratch_ids;

    return 0;
}
