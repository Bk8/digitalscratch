/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*               libdigitalscratch: the Digital Scratch engine.               */
/*                                                                            */
/*                                                                            */
/*-------------------------------------------------------( mixvibes_vinyl.h )-*/
/*                                                                            */
/*  Copyright (C) 2003-2016                                                   */
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
/*        Mixvibes_vinyl class : define a Mixvibes DVS timecode vinyl         */
/*                                                                            */
/*============================================================================*/

#pragma once

#include <string>
#include "dscratch_parameters.h"
#include "coded_vinyl.h"
#include "digital_scratch_api.h"

// Mixvibes vinyl sinusoidal frequency (Hz) (@33 rpm)
#define MIXVIBES_SINUSOIDAL_FREQ 1300.0f

// Mixvibes vinyl sinusoidal frequency (Hz) (@45 rpm)
#define MIXVIBES_SINUSOIDAL_FREQ_45RPM 1755.0f

/**
 * Define a Mixvibes DVS timecode vinyl class.\n
 * @author Julien Rosener
 */
class Mixvibes_vinyl : public Coded_vinyl
{
    public:
        Mixvibes_vinyl(unsigned int sample_rate);
        virtual ~Mixvibes_vinyl();

    public:
        float get_speed();
        float get_volume();
};
