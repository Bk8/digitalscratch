/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch Player                           */
/*                                                                            */
/*                                                                            */
/*-----------------------------------------------( manual_control_process.h )-*/
/*                                                                            */
/*  Copyright (C) 2003-2015                                                   */
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
/* Behavior class: determine playback parametrs based on keyboard and gui     */
/*                 buttons                                                    */
/*                                                                            */
/*============================================================================*/

#pragma once

#include <iostream>
#include <QSharedPointer>

#include "playback_parameters.h"
#include "application_const.h"

using namespace std;

class Manual_control_process
{
 private:
    QSharedPointer<Playback_parameters> params;
    float speed;

 public:
    Manual_control_process(QSharedPointer<Playback_parameters> &in_param);
    virtual ~Manual_control_process();

    bool run();
    void inc_speed(float in_speed_inc);
    void reset_speed_to_100p();
};