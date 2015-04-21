/*============================================================================*/
/*                                                                            */
/*                                                                            */
/*                           Digital Scratch System                           */
/*                                                                            */
/*                                                                            */
/*--------------------------------------------------( digital_scratch_api.h )-*/
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
/*              API header file for the Digital Scratch lib                   */
/*                                                                            */
/*============================================================================*/

#pragma once

#ifdef WIN32
    #ifdef DLLIMPORT
        #undef DLLIMPORT
    #endif
    #define DLLIMPORT __declspec (dllexport)
#else
    #define DLLIMPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/**************************** Error codes *************************************/

enum DSCRATCH_STATUS
{
    DSCRATCH_SUCCESS,
    DSCRATCH_ERROR
};

/**< This is the speed value if no new value is found. */
#define NO_NEW_SPEED_FOUND -99.0

/**< This is the volume value if no new value is found. */
#define NO_NEW_VOLUME_FOUND -99.0

/******************************************************************************/
/********************* Supported timecoded vinyl type *************************/

enum DSCRATCH_VINYLS
{
    FINAL_SCRATCH = 0,
    SERATO,
    MIXVIBES,
    NB_DSCRATCH_VINYLS
};

/******************************************************************************/
/************************** Supported base RPM ********************************/

#define RPM_33 33
#define RPM_45 45

/******************************************************************************/
/************ API functions: create, delete, provide datas,... ****************/

/**< Used by API functions to identify the turntable. */
typedef int DSCRATCH_HANDLE;

/**
 * Create a new turntable.
 *
 * @param name is the name you want to give to the turntable.
 * @param coded_vinyl_type is the type of timecoded vinyl you want to use
 *        (e.g. FINAL_SCRATCH_VINYL, see above).
 * @param sample rate is the rate of the timecoded input signal.
 * @param handle is used to identify the turntable.
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_create_turntable(const char         *name,
                                                    DSCRATCH_VINYLS     coded_vinyl_type,
                                                    const unsigned int  sample_rate,
                                                    DSCRATCH_HANDLE    *handle);

/**
 * Remove the specified turntable from turntable list and delete (deallocate
 * memory) it.
 *
 * @param handle is used to identify the turntable.
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_delete_turntable(DSCRATCH_HANDLE handle);

/**
 * Provide samples recorded from turntable (with timecoded vinyl) and analyze
 * them.
 *
 * @param handle is used to identify the turntable.
 * @param input_samples_1 is a table containing samples from left channel.
 * @param input_samples_2 is a table containing samples from right channel.
 * @param nb_frames is the size (number of element) of input_samples_1.
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 *
 * @note Warning: input_samples_1 and input_samples_2 must have the same number
 *                of elements.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_analyze_recorded_datas(DSCRATCH_HANDLE  handle,
                                                          const float     *input_samples_1,
                                                          const float     *input_samples_2,
                                                          int              nb_frames);


/**
 * Same as dscratch_analyze_recorded_datas() but with interleaved datas.
 * E.g. 0.12 0.98 -0.38 0.68 ...
 *      l[0] r[0] l[1]  r[1] ... (l=left, r=right)
 *
 * @param handle is used to identify the turntable.
 * @param nb_channels is the number of channels used for interleaved datas (for
 *                    a stereo input it is 2, for a bi-stereo input it is 4,...).
 * @param left_index is the index of the first left channel you want to use (it
 *                   is 0 if you want to use the first stereo input, 2 for the
 *                   second one,...).
 * @param right_index is the index of the first right channel you want to use (it
 *                    is 1 if you want to use the first stereo input, 3 for the
 *                    second one,...).
 * @param input_samples_interleaved is a table containing interleaved samples
 *        from left and right channels.
 * @param nb_frames is the size (number of element) of input_samples_interleaved
 *        divided by nb_channels.
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 *
 */
DLLIMPORT DSCRATCH_STATUS dscratch_analyze_recorded_datas_interleaved(DSCRATCH_HANDLE  handle,
                                                                      int              nb_channels,
                                                                      int              left_index,
                                                                      int              right_index,
                                                                      float           *input_samples_interleaved,
                                                                      int              nb_frames);

/**
 * Provide playing parameters (only relevant if dscratch_analyze_recorded_datas()
 * was called). Playing parameters are:
 *      - speed of the vinyl disc (sign is the direction).
 *      - volume of the sound (dependant of the speed).
 *
* @param handle is used to identify the turntable.
 *
 * @param speed will be returned, this is the speed of the vinyl disc.
 *        1.0 should be mapped to 0.0% of your real turntable.
 *        If the speed is a negative value, it means that vinyl is playing
 *        backward.
 *        NO_NEW_SPEED_FOUND is returned if no speed is found.
 *
 * @param volume will be returned, this is the volume of the sound you want to
 *        play. Indeed, the volume of the sound is dependant of the speed, so
 *        the more is the speed the more will be the volume.
 *        0.0 correspond to mute. 1.0 should be mapped to 80% of the maximum of
 *        the volume for example, in fact DigitalScratch can return for example
 *        a volume equal to 1.26.
 *        NO_NEW_VOLUME_FOUND is returned if no volume is found.
 *
 * @return 0 if playing parameters are found, otherwise 1.
 * @return DSCRATCH_SUCCESS if playing parameters are found.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_get_playing_parameters(DSCRATCH_HANDLE  handle,
                                                          float           *speed,
                                                          float           *volume);

/**
 * Get number of turntable registered in DigitalScratch.
 *
 * @return number of turntable.
 */
DLLIMPORT int dscratch_get_number_of_turntables();

/**
 * Get DigitalScratch version.
 *
 * @return a const string containing version number.
 *
 */
DLLIMPORT const char *dscratch_get_version();

/**
 * Display on stdout informations about specified turntable.
 *
 * @param handle is used to identify the turntable.
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_display_turntable(DSCRATCH_HANDLE handle);

/**
 * Get name of specified turntable.
 *
 * @param handle is used to identify the turntable.
 * @param name buffer in which will be putted the name of the turntable.
 *
 * @note This function will allocate (malloc) the buffer in which it will put
 *       the name, so do not forget to deallocate it (free).
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
// FIXME: is it necessary to have a turntable name ?
DLLIMPORT DSCRATCH_STATUS dscratch_get_turntable_name(DSCRATCH_HANDLE   handle,
                                                      char            **turntable_name);

/**
 * Get vinyl type used for specified turntable.
 *
 * @param handle is used to identify the turntable.
 * @param vinyl_type is the vinyl type.
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_get_turntable_vinyl_type(DSCRATCH_HANDLE   handle,
                                                            DSCRATCH_VINYLS   *vinyl_type);

/**
 * Transform a vinyl type to an explicit string name.
 *
 * @param vinyl_type is the vinyl type.
 *
 * @return A full string name corresponding to the type.
 */
DLLIMPORT const char* dscratch_get_vinyl_name_from_type(DSCRATCH_VINYLS vinyl_type);

/**
 * Get default vinyl type.
 *
 * @return the default vinyl type (Serato vinyl).
 */
DLLIMPORT DSCRATCH_VINYLS dscratch_get_default_vinyl_type();


/**
 * Change vinyl type without deleting and recreating engine.
 *
 * @param handle is used to identify the turntable.
 * @param vinyl_type is the type of vinyl (@see DSCRATCH_VINYLS).
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_change_vinyl_type(DSCRATCH_HANDLE  handle,
                                                     DSCRATCH_VINYLS  vinyl_type);



/******************************************************************************/
/**** API functions: General motion detection configuration parameters ********/
/**
 * Set the coefficient to be multiplied to input timecoded signal.
 *
 * @param handle is used to identify the turntable.
 * @param coeff is the value to be multiplied to input samples.
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_set_input_amplify_coeff(DSCRATCH_HANDLE handle,
                                                           int             coeff);

/**
 * Get the coefficient to be multiplied to input timecoded signal.
 *
 * @param handle is used to identify the turntable.
 *
 * @return the coefficient used to be multiplied to input samples.
 */
DLLIMPORT int dscratch_get_input_amplify_coeff(DSCRATCH_HANDLE handle);

/**
 * Get the default coefficient to be multiplied to input timecoded signal.
 *
 * @return the coefficient used to be multiplied to input samples.
 */
DLLIMPORT int dscratch_get_default_input_amplify_coeff();

/**
 * Set the number of RPM.
 *
 * @param handle is used to identify the turntable.
 * @param rpm is the number of RPM of the turntable (45 or 33).
 *
 * @return DSCRATCH_SUCCESS if all is OK.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_set_rpm(DSCRATCH_HANDLE    handle,
                                           unsigned short int rpm);

/**
 * Get the turntable RPM value.
 *
 * @param handle is used to identify the turntable.
 *
 * @return the number of rpm (45 or 33).
 */
DLLIMPORT unsigned short int dscratch_get_rpm(DSCRATCH_HANDLE handle);

/**
 * Get the default number of RPM.
 *
 * @return the default number of rpm (45 or 33).
 */
DLLIMPORT unsigned short int dscratch_get_default_rpm();

/**
  * Getter/Setter for the minimal acceptable amplitude for a normal speed.
  */
DLLIMPORT DSCRATCH_STATUS dscratch_set_min_amplitude_for_normal_speed(DSCRATCH_HANDLE handle, float amplitude);
DLLIMPORT float dscratch_get_min_amplitude_for_normal_speed(DSCRATCH_HANDLE handle);
DLLIMPORT float dscratch_get_default_min_amplitude_for_normal_speed();
DLLIMPORT float dscratch_get_default_min_amplitude_for_normal_speed_from_vinyl_type(DSCRATCH_VINYLS vinyl_type);

/**
 * Getter/Setter for the minimal detectable amplitude.
 */
DLLIMPORT DSCRATCH_STATUS dscratch_set_min_amplitude(DSCRATCH_HANDLE handle, float amplitude);
DLLIMPORT float dscratch_get_min_amplitude(DSCRATCH_HANDLE handle);
DLLIMPORT float dscratch_get_default_min_amplitude();
DLLIMPORT float dscratch_get_default_min_amplitude_from_vinyl_type(DSCRATCH_VINYLS vinyl_type);

#ifdef __cplusplus
}
#endif

