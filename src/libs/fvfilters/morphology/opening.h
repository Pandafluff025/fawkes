
/***************************************************************************
 *  opening.h - header for morphological opening filter
 *
 *  Created: Mon Jun 05 13:39:30 2006
 *  Copyright  2005-2007  Tim Niemueller [www.niemueller.de]
 *
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. A runtime exception applies to
 *  this software (see LICENSE.GPL_WRE file mentioned below for details).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL_WRE file in the doc directory.
 */

#ifndef _FIREVISION_FILTER_MORPHOLOGY_OPENING_H_
#define _FIREVISION_FILTER_MORPHOLOGY_OPENING_H_

#include <fvfilters/morphology/morphologicalfilter.h>

namespace firevision {

class FilterDilation;
class FilterErosion;

class FilterOpening : public MorphologicalFilter
{
public:
	FilterOpening();
	virtual ~FilterOpening();

	virtual void set_src_buffer(unsigned char *buf,
	                            ROI           *roi,
	                            orientation_t  ori        = ORI_HORIZONTAL,
	                            unsigned int   buffer_num = 0);

	virtual void set_src_buffer(unsigned char *buf, ROI *roi, unsigned int buffer_num);

	virtual void set_dst_buffer(unsigned char *buf, ROI *roi);

	virtual void set_structuring_element(unsigned char *se,
	                                     unsigned int   se_width,
	                                     unsigned int   se_height,
	                                     unsigned int   se_anchor_x,
	                                     unsigned int   se_anchor_y);

	virtual void apply();

private:
	FilterDilation *dilate;
	FilterErosion  *erode;
};

} // end namespace firevision

#endif
