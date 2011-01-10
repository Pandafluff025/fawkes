
/***************************************************************************
 *  rrdweb_thread.cpp - RRD Webview Thread
 *
 *  Created: Tue Dec 21 01:05:45 2010
 *  Copyright  2006-2010  Tim Niemueller [www.niemueller.de]
 *
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

#include "rrdweb_thread.h"
#include "rrdweb_processor.h"

#include <webview/url_manager.h>
#include <webview/nav_manager.h>

using namespace fawkes;

#define RRD_URL_PREFIX "/rrd"

/** @class RRDWebThread "rrd_thread.h"
 * RRD Webview Thread.
 * This thread queries RRD graphs from RRDManager obtained via RRDAspect
 * and displays them on a Webview page.
 *
 * @author Tim Niemueller
 */

/** Constructor. */
RRDWebThread::RRDWebThread()
  : Thread("RRDWebThread", Thread::OPMODE_WAITFORWAKEUP)
{
}


/** Destructor. */
RRDWebThread::~RRDWebThread()
{
}


void
RRDWebThread::init()
{
  __processor  = new RRDWebRequestProcessor(rrd_manager, logger, RRD_URL_PREFIX);
  webview_url_manager->register_baseurl(RRD_URL_PREFIX, __processor);
  webview_nav_manager->add_nav_entry(RRD_URL_PREFIX, "RRD Graphs");
}


void
RRDWebThread::finalize()
{
  delete __processor;
}


void
RRDWebThread::loop()
{
}

