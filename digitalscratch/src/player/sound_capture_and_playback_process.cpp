/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch Player                           */
/*                                                                            */
/*                                                                            */
/*---------------------------------( sound_capture_and_playback_process.cpp )-*/
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
/* Behavior class: process called each time there are new captured data and   */
/*                 playable data are ready.                                   */
/*                                                                            */
/*============================================================================*/

#include <QtDebug>

#include "sound_capture_and_playback_process.h"
#include "application_logging.h"


Sound_capture_and_playback_process::Sound_capture_and_playback_process(QList<QSharedPointer<Timecode_control_process>>     &in_tcode_controls,
                                                                       QList<QSharedPointer<Audio_track_playback_process>> &in_playbacks,
                                                                       QSharedPointer<Sound_driver_access_rules>           &in_sound_card,
                                                                       unsigned short int                               in_nb_decks)
{
    if (in_tcode_controls.count() == 0 ||
        in_playbacks.count()      == 0 ||
        in_sound_card.data()      == NULL)
    {
        qCWarning(DS_PLAYBACK) << "bad input parameters";
        return;
    }
    else
    {
        this->tcode_controls = in_tcode_controls;
        this->playbacks      = in_playbacks;
        this->sound_card     = in_sound_card;
        this->nb_decks       = in_nb_decks;
    }

    return;
}

Sound_capture_and_playback_process::~Sound_capture_and_playback_process()
{
    return;
}

bool
Sound_capture_and_playback_process::run(unsigned short int in_nb_buffer_frames)
{
    float *input_buffer_1  = NULL;
    float *input_buffer_2  = NULL;
    float *input_buffer_3  = NULL;
    float *input_buffer_4  = NULL;

    float *output_buffer_1  = NULL;
    float *output_buffer_2  = NULL;
    float *output_buffer_3  = NULL;
    float *output_buffer_4  = NULL;

    // Get sound card buffers. // TODO : a ne faire que si mode = timecode
    if(this->sound_card->get_input_buffers(in_nb_buffer_frames,
                                           &input_buffer_1,
                                           &input_buffer_2,
                                           &input_buffer_3,
                                           &input_buffer_4) == false)
    {
        qCWarning(DS_SOUNDCARD) << "can not get input buffers";
        return false;
    }
    if(this->sound_card->get_output_buffers(in_nb_buffer_frames,
                                            &output_buffer_1,
                                            &output_buffer_2,
                                            &output_buffer_3,
                                            &output_buffer_4) == false)
    {
        qCWarning(DS_SOUNDCARD) << "can not get output buffers";
        return false;
    }

    // Analyze captured data. // TODO : a ne faire que si mode = timecode
    if (this->tcode_controls[0]->run(in_nb_buffer_frames,
                                 input_buffer_1,
                                 input_buffer_2) == false)
    {
        qCWarning(DS_PLAYBACK) << "timecode analysis failed for deck 1";
        return false;
    }
    if ((this->nb_decks > 1) &&
       (this->tcode_controls[1]->run(in_nb_buffer_frames,
                                     input_buffer_3,
                                     input_buffer_4) == false))
    {
        qCWarning(DS_PLAYBACK) << "timecode analysis failed for deck 2";
        return false;
    }

    // Faire un cas particulier pour mode = manual.

    // Play data.
    if (this->playbacks[0]->run(in_nb_buffer_frames,
                            output_buffer_1,
                            output_buffer_2) == false)
    {
        qCWarning(DS_PLAYBACK) << "playback process failed for deck 1";
        return false;
    }
    if ((this->nb_decks > 1) &&
        (this->playbacks[1]->run(in_nb_buffer_frames,
                            output_buffer_3,
                            output_buffer_4) == false))
    {
        qCWarning(DS_PLAYBACK) << "playback process failed for deck 2";
        return false;
    }

    return true;
}
