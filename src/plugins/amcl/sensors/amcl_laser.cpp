/***************************************************************************
 *  amcl_laser.cpp: AMCL laser routines
 *
 *  Created: Thu May 24 18:50:35 2012
 *  Copyright  2000  Brian Gerkey
 *             2000  Kasper Stoy
 *             2012  Tim Niemueller [www.niemueller.de]
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

/*  From:
 *  Player - One Hell of a Robot Server (LGPL)
 *  Copyright (C) 2000  Brian Gerkey   &  Kasper Stoy
 *                      gerkey@usc.edu    kaspers@robotics.usc.edu
 */
///////////////////////////////////////////////////////////////////////////
// Desc: AMCL laser routines
// Author: Andrew Howard
// Date: 6 Feb 2003
///////////////////////////////////////////////////////////////////////////

#include <sys/types.h> // required by Darwin

#include <math.h>
#include <stdlib.h>
#ifdef USE_ASSERT_EXCEPTION
#	include <core/assert_exception.h>
#else
#	include <assert.h>
#endif
#include "amcl_laser.h"

#include <unistd.h>

using namespace amcl;

/// @cond EXTERNAL

////////////////////////////////////////////////////////////////////////////////
// Default constructor
AMCLLaser::AMCLLaser(size_t max_beams, map_t *map) : AMCLSensor()
{
	this->time = 0.0;

	this->max_beams = max_beams;
	this->map       = map;

	this->model_type   = LASER_MODEL_BEAM;
	this->z_hit        = .95;
	this->z_short      = .05;
	this->z_max        = .05;
	this->z_rand       = .05;
	this->sigma_hit    = .2;
	this->lambda_short = .1;
	this->chi_outlier  = 0.0;

	return;
}

void
AMCLLaser::SetModelBeam(double z_hit,
                        double z_short,
                        double z_max,
                        double z_rand,
                        double sigma_hit,
                        double lambda_short,
                        double chi_outlier)
{
	this->model_type   = LASER_MODEL_BEAM;
	this->z_hit        = z_hit;
	this->z_short      = z_short;
	this->z_max        = z_max;
	this->z_rand       = z_rand;
	this->sigma_hit    = sigma_hit;
	this->lambda_short = lambda_short;
	this->chi_outlier  = chi_outlier;
}

void
AMCLLaser::SetModelLikelihoodField(double z_hit,
                                   double z_rand,
                                   double sigma_hit,
                                   double max_occ_dist)
{
	this->model_type = LASER_MODEL_LIKELIHOOD_FIELD;
	this->z_hit      = z_hit;
	this->z_rand     = z_rand;
	this->sigma_hit  = sigma_hit;

	map_update_cspace(this->map, max_occ_dist);
}

////////////////////////////////////////////////////////////////////////////////
// Apply the laser sensor model
bool
AMCLLaser::UpdateSensor(pf_t *pf, AMCLSensorData *data)
{
	//AMCLLaserData *ndata;

	//ndata = (AMCLLaserData*) data;
	if (this->max_beams < 2)
		return false;

	// Apply the laser sensor model
	if (this->model_type == LASER_MODEL_BEAM)
		pf_update_sensor(pf, (pf_sensor_model_fn_t)BeamModel, data);
	else if (this->model_type == LASER_MODEL_LIKELIHOOD_FIELD)
		pf_update_sensor(pf, (pf_sensor_model_fn_t)LikelihoodFieldModel, data);
	else
		pf_update_sensor(pf, (pf_sensor_model_fn_t)BeamModel, data);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Determine the probability for the given pose
double
AMCLLaser::BeamModel(AMCLLaserData *data, pf_sample_set_t *set)
{
	AMCLLaser *self         = static_cast<AMCLLaser *>(data->sensor);
	double     total_weight = 0.0;

	// Compute the sample weights
	for (int j = 0; j < set->sample_count; j++) {
		pf_sample_t *sample = set->samples + j;
		pf_vector_t  pose{sample->pose};

		// Take account of the laser pose relative to the robot
		pose = pf_vector_coord_add(self->laser_pose, pose);

		double p = 1.0;

		int step = (data->range_count - 1) / (self->max_beams - 1);
		for (int i = 0; i < data->range_count; i += step) {
			double obs_range   = data->ranges[i][0];
			double obs_bearing = data->ranges[i][1];

			// Compute the range according to the map
			double map_range =
			  map_calc_range(self->map, pose.v[0], pose.v[1], pose.v[2] + obs_bearing, data->range_max);
			double pz = 0.0;

			// Part 1: good, but noisy, hit
			double z = obs_range - map_range;
			pz += self->z_hit * exp(-(z * z) / (2 * self->sigma_hit * self->sigma_hit));

			// Part 2: short reading from unexpected obstacle (e.g., a person)
			if (z < 0)
				pz += self->z_short * self->lambda_short * exp(-self->lambda_short * obs_range);

			// Part 3: Failure to detect obstacle, reported as max-range
			if (obs_range == data->range_max)
				pz += self->z_max * 1.0;

			// Part 4: Random measurements
			if (obs_range < data->range_max)
				pz += self->z_rand * 1.0 / data->range_max;

			// TODO: outlier rejection for short readings

			//assert(pz <= 1.0);
			//assert(pz >= 0.0);
			if ((pz < 0.) || (pz > 1.))
				pz = 0.;

			//      p *= pz;
			// here we have an ad-hoc weighting scheme for combining beam probs
			// works well, though...
			p += pz * pz * pz;
		}

		sample->weight *= p;
		total_weight += sample->weight;
	}

	return (total_weight);
}

double
AMCLLaser::LikelihoodFieldModel(AMCLLaserData *data, pf_sample_set_t *set)
{
	AMCLLaser   *self;
	int          i, j;
	double       z, pz;
	double       obs_range, obs_bearing;
	double       total_weight;
	pf_sample_t *sample;
	pf_vector_t  pose;
	pf_vector_t  hit;

	self = (AMCLLaser *)data->sensor;

	total_weight = 0.0;

	// Compute the sample weights
	for (j = 0; j < set->sample_count; j++) {
		sample = set->samples + j;
		pose   = sample->pose;

		// Take account of the laser pose relative to the robot
		pose = pf_vector_coord_add(self->laser_pose, pose);

		double p = 1.0;

		// Pre-compute a couple of things
		double z_hit_denom = 2 * self->sigma_hit * self->sigma_hit;
		double z_rand_mult = 1.0 / data->range_max;

		int step = (data->range_count - 1) / (self->max_beams - 1);
		for (i = 0; i < data->range_count; i += step) {
			obs_range   = data->ranges[i][0];
			obs_bearing = data->ranges[i][1];

			// This model ignores max range readings
			if (obs_range >= data->range_max)
				continue;

			pz = 0.0;

			// Compute the endpoint of the beam
			hit.v[0] = pose.v[0] + obs_range * cos(pose.v[2] + obs_bearing);
			hit.v[1] = pose.v[1] + obs_range * sin(pose.v[2] + obs_bearing);

			// Convert to map grid coords.
			int mi, mj;
			mi = MAP_GXWX(self->map, hit.v[0]);
			mj = MAP_GYWY(self->map, hit.v[1]);

			// Part 1: Get distance from the hit to closest obstacle.
			// Off-map penalized as max distance
			if (!MAP_VALID(self->map, mi, mj))
				z = self->map->max_occ_dist;
			else
				z = self->map->cells[MAP_INDEX(self->map, mi, mj)].occ_dist;
			// Gaussian model
			// NOTE: this should have a normalization of 1/(sqrt(2pi)*sigma)
			pz += self->z_hit * exp(-(z * z) / z_hit_denom);
			// Part 2: random measurements
			pz += self->z_rand * z_rand_mult;

			// TODO: outlier rejection for short readings

			//assert(pz <= 1.0);
			//assert(pz >= 0.0);
			if ((pz < 0.) || (pz > 1.))
				pz = 0.;

			//      p *= pz;
			// here we have an ad-hoc weighting scheme for combining beam probs
			// works well, though...
			p += pz * pz * pz;
		}

		sample->weight *= p;
		total_weight += sample->weight;
	}

	return (total_weight);
}

/// @endcond
