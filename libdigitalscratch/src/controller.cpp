/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch System                           */
/*                                                                            */
/*                                                                            */
/*---------------------------------------------------------( controller.cpp )-*/
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
/*                Controller class : define a player controller               */
/*                                                                            */
/*============================================================================*/

#include <iostream>
#include <cstdio>
#include <string>

using namespace std;

#include "log.h"
#include "controller.h"

Controller::Controller()
{
    this->speed    = 0.0;
    this->volume   = 0.0;
    this->set_playing_parameters_ready(false);
}

Controller::~Controller()
{
}

bool Controller::get_playing_parameters(float *speed,
                                        float *volume)
{
    if (this->playing_parameters_ready == true)
    {
        *speed    = this->speed;
        *volume   = this->volume;

        return true;
    }
    else
    {
        return false;
    }
}

bool Controller::set_playing_parameters_ready(bool flag)
{
    this->playing_parameters_ready = flag;

    return true;
}
