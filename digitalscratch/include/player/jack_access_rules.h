/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch Player                           */
/*                                                                            */
/*                                                                            */
/*----------------------------------------------( sound_card_access_rules.h )-*/
/*                                                                            */
/*  Copyright (C) 2003-2012                                                   */
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
/*        Behavior class: access sound card (open, close, list, ...)          */
/*                                                                            */
/*============================================================================*/

#ifndef JACK_ACCESS_RULES_H_
#define JACK_ACCESS_RULES_H_

#include <iostream>
#include <QObject>
#include <QString>
#include <sound_card_access_rules.h>
#include "jack/jack.h"
#include <application_const.h>
typedef jack_client_t *     AUDIO_STREAM_TYPE;
typedef JackProcessCallback AUDIO_CALLBACK_TYPE;
typedef jack_nframes_t      AUDIO_CALLBACK_NB_FRAMES_TYPE;

#define CLIENT_NAME   "digitalscratch"
#define INPUT_PORT_1  "left_turntable-left_channel"
#define INPUT_PORT_2  "left_turntable-right_channel"
#define INPUT_PORT_3  "right_turntable-left_channel"
#define INPUT_PORT_4  "right_turntable-right_channel"
#define OUTPUT_PORT_1 "mixer_left_side-left_speaker"
#define OUTPUT_PORT_2 "mixer_left_side-right_speaker"
#define OUTPUT_PORT_3 "mixer_right_side-left_speaker"
#define OUTPUT_PORT_4 "mixer_right_side-right_speaker"

using namespace std;

class Jack_access_rules : public Sound_card_access_rules
{
    Q_OBJECT

 private:
    AUDIO_STREAM_TYPE stream;

 public:
    jack_port_t  **input_port;
    jack_port_t  **output_port;

 public:
    Jack_access_rules(unsigned short int in_nb_channels);
    virtual ~Jack_access_rules();

 public:
    bool start(void *in_callback_param);
    bool restart();
    bool stop();
    bool is_running();
    bool get_input_buffers(unsigned short int   in_nb_buffer_frames,
                           float              **out_buffer_1,
                           float              **out_buffer_2,
                           float              **out_buffer_3,
                           float              **out_buffer_4);
    bool get_output_buffers(unsigned short int   in_nb_buffer_frames,
                            float              **out_buffer_1,
                            float              **out_buffer_2,
                            float              **out_buffer_3,
                            float              **out_buffer_4);
};

#endif /* JACK_ACCESS_RULES_H_ */