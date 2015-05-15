/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch System                           */
/*                                                                            */
/*                                                                            */
/*----------------------------------------------------------( coded_vinyl.h )-*/
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
/*            Coded_vynil class : define a coded vinyl disk                   */
/*                                                                            */
/*============================================================================*/

#pragma once

#include <string>
#include <QVector>

#include "dscratch_parameters.h"
#include "digital_scratch_api.h"
#include "fir_filter.h"
#include "unwrapper.h"
#include "iir_filter.h"

#define DEFAULT_RPM RPM_33

/**
 * Define a Coded_vinyl class.\n
 * A coded vinyl is the definition of a vinyl disc with a timecoded signal.
 * @author Julien Rosener
 */
class Coded_vinyl
{
 protected:
    float min_amplitude;

 private:
    unsigned int         sample_rate;
    dscratch_vinyl_rpm_t rpm;
    bool                 is_reverse_direction;

    // Frequency and amplitude analysis.
    FIR_filter diff_FIR;
    Unwrapper  unwraper;
    IIR_filter speed_IIR;
    IIR_filter amplitude_IIR;
    double     filtered_freq_inst;
    double     filtered_amplitude_inst;

 public:
    Coded_vinyl(unsigned int sample_rate);
    virtual ~Coded_vinyl();

 public:
    void run_recording_data_analysis(const QVector<float> &input_samples_1,
                                     const QVector<float> &input_samples_2);

    float get_speed();
    float get_volume();

    bool set_sample_rate(unsigned int sample_rate);
    unsigned int get_sample_rate();

    bool set_rpm(dscratch_vinyl_rpm_t rpm);
    dscratch_vinyl_rpm_t get_rpm();

    void  set_min_amplitude(float amplitude);
    float get_min_amplitude();
    virtual float get_default_min_amplitude() = 0;

    virtual int get_sinusoidal_frequency() = 0;

 protected:
    bool set_reverse_direction(bool is_reverse_direction);
    bool get_reverse_direction();
};
