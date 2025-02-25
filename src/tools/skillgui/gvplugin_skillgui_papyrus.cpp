
/***************************************************************************
 *  gvplugin_skillgui_papyrus.cpp - Graphviz plugin for Skill GUI using
 *                                  the Cairo-based Papyrus scene graph lib
 *
 *  Created: Tue Dec 16 14:36:51 2008
 *  Copyright  2008-2009  Tim Niemueller [www.niemueller.de]
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

#include "gvplugin_skillgui_papyrus.h"

#include <utils/math/angle.h>
#include <utils/time/tracker.h>

#include <algorithm>
#include <cstdio>
#include <gvplugin_device.h>
#include <gvplugin_render.h>

#define NOEXPORT __attribute__((visibility("hidden")))

NOEXPORT SkillGuiGraphViewport *sggvp_ = NULL;

NOEXPORT std::valarray<double> skillgui_render_dashed_(6., 1);
NOEXPORT std::valarray<double> skillgui_render_dotted_((double[]){2., 6.}, 2);

#ifdef USE_GVPLUGIN_TIMETRACKER
NOEXPORT fawkes::TimeTracker tt_;
NOEXPORT unsigned int        ttc_page_      = tt_.add_class("Page");
NOEXPORT unsigned int        ttc_beginpage_ = tt_.add_class("Begin Page");
NOEXPORT unsigned int        ttc_ellipse_   = tt_.add_class("Ellipse");
NOEXPORT unsigned int        ttc_bezier_    = tt_.add_class("Bezier");
NOEXPORT unsigned int        ttc_polygon_   = tt_.add_class("Polygon");
NOEXPORT unsigned int        ttc_polyline_  = tt_.add_class("Polyline");
NOEXPORT unsigned int        ttc_text_      = tt_.add_class("Text");
NOEXPORT unsigned int        ttc_text_1_    = tt_.add_class("Text 1");
NOEXPORT unsigned int        ttc_text_2_    = tt_.add_class("Text 2");
NOEXPORT unsigned int        ttc_text_3_    = tt_.add_class("Text 3");
NOEXPORT unsigned int        ttc_text_4_    = tt_.add_class("Text 4");
NOEXPORT unsigned int        ttc_text_5_    = tt_.add_class("Text 5");
NOEXPORT unsigned int        tt_count_      = 0;
NOEXPORT unsigned int        num_ellipse_   = 0;
NOEXPORT unsigned int        num_bezier_    = 0;
NOEXPORT unsigned int        num_polygon_   = 0;
NOEXPORT unsigned int        num_polyline_  = 0;
NOEXPORT unsigned int        num_text_      = 0;
#endif

static void
skillgui_device_init(GVJ_t *firstjob)
{
	Glib::RefPtr<const Gdk::Screen> s = sggvp_->get_screen();
	firstjob->device_dpi.x            = s->get_resolution();
	firstjob->device_dpi.y            = s->get_resolution();
	firstjob->device_sets_dpi         = true;

	Gtk::Allocation alloc = sggvp_->get_allocation();
	firstjob->width       = alloc.get_width();
	firstjob->height      = alloc.get_height();

	firstjob->fit_mode = TRUE;
}

static void
skillgui_device_finalize(GVJ_t *firstjob)
{
	sggvp_->set_gvjob(firstjob);

	firstjob->context          = (void *)sggvp_;
	firstjob->external_context = TRUE;

	// Render!
	(firstjob->callbacks->refresh)(firstjob);
}

static inline Papyrus::Fill::pointer
skillgui_render_solidpattern(gvcolor_t *color)
{
	Cairo::RefPtr<Cairo::SolidPattern> pattern;
	pattern = Cairo::SolidPattern::create_rgba(color->u.RGBA[0],
	                                           color->u.RGBA[1],
	                                           color->u.RGBA[2],
	                                           color->u.RGBA[3]);
	return Papyrus::Fill::create(pattern);
}

static inline Papyrus::Stroke::pointer
skillgui_render_stroke(obj_state_t *obj)
{
	Papyrus::Stroke::pointer stroke;

	Cairo::RefPtr<Cairo::SolidPattern> pattern;
	pattern = Cairo::SolidPattern::create_rgba(obj->pencolor.u.RGBA[0],
	                                           obj->pencolor.u.RGBA[1],
	                                           obj->pencolor.u.RGBA[2],
	                                           obj->pencolor.u.RGBA[3]);

	stroke = Papyrus::Stroke::create(pattern, obj->penwidth);

	if (obj->pen == PEN_DASHED) {
		stroke->set_dash(skillgui_render_dashed_);
	} else if (obj->pen == PEN_DOTTED) {
		stroke->set_dash(skillgui_render_dotted_);
	}

	return stroke;
}

static void
skillgui_render_begin_page(GVJ_t *job)
{
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_start(ttc_page_);
	tt_.ping_start(ttc_beginpage_);
#endif
	SkillGuiGraphViewport *gvp = static_cast<SkillGuiGraphViewport *>(job->context);
	gvp->clear();
	Gtk::Allocation alloc    = sggvp_->get_allocation();
	float           bbwidth  = job->bb.UR.x - job->bb.LL.x;
	float           bbheight = job->bb.UR.y - job->bb.LL.y;
	float           avwidth  = alloc.get_width();
	float           avheight = alloc.get_height();
	float           zoom_w   = avwidth / bbwidth;
	float           zoom_h   = avheight / bbheight;
	float           zoom     = std::min(zoom_w, zoom_h);

	float translate_x = 0;
	float translate_y = 0;

	if (bbwidth > avwidth || bbheight > avheight) {
		float zwidth  = bbwidth * zoom;
		float zheight = bbheight * zoom;
		translate_x += (avwidth - zwidth) / 2.;
		translate_y += (avheight - zheight) / 2.;
	} else {
		zoom = 1.0;
		translate_x += (avwidth - bbwidth) / 2.;
		translate_y += (avheight - bbheight) / 2.;
	}

	gvp->set_bb(bbwidth, bbheight);
	gvp->set_pad(job->pad.x, job->pad.y);
	gvp->set_scale(zoom);
	gvp->set_translation(translate_x, translate_y);

	if (!gvp->scale_override()) {
		gvp->get_affine()->set_translate(translate_x + job->pad.x, translate_y + job->pad.y);
		gvp->get_affine()->set_scale(zoom);
	}

	/*
  char *graph_translate_x = agget(obj->u.g, (char *)"trans_x");
  if (graph_translate_x && (strlen(graph_translate_x) > 0)) {
    float translate_x = atof(graph_translate_x) * zoom;
    float translate_y = atof(graph_translate_y) * job->scale.x;
  }
  */
#ifdef USE_GVPLUGIN_TIMETRACKER
	num_ellipse_  = 0;
	num_bezier_   = 0;
	num_polygon_  = 0;
	num_polyline_ = 0;
	num_text_     = 0;

	tt_.ping_end(ttc_beginpage_);
#endif
}

static void
skillgui_render_end_page(GVJ_t *job)
{
	//SkillGuiGraphViewport *gvp = (SkillGuiGraphViewport *)job->context;
	//gvp->queue_draw();
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_end(ttc_page_);
	if (++tt_count_ >= 10) {
		tt_count_ = 0;
		tt_.print_to_stdout();

		printf("Num Ellipse:   %u\n"
		       "Num Bezier:    %u\n"
		       "Num Polygon:   %u\n"
		       "Num Polyline:  %u\n"
		       "Num Text:      %u\n",
		       num_ellipse_,
		       num_bezier_,
		       num_polygon_,
		       num_polyline_,
		       num_text_);
	}
#endif
}

static void
skillgui_render_textpara(GVJ_t *job, pointf p, textpara_t *para)
{
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_start(ttc_text_);
	++num_text_;
#endif
	SkillGuiGraphViewport *gvp = (SkillGuiGraphViewport *)job->context;
	obj_state_t           *obj = job->obj;

	switch (para->just) {
	case 'r': p.x -= para->width; break;
	case 'l': p.x -= 0.0; break;
	case 'n':
	default: p.x -= para->width / 2.0; break;
	}

	p.y += para->height / 2.0 + para->yoffset_centerline;
	//p.y -= para->yoffset_centerline;
	//p.y += para->yoffset_centerline; // + para->yoffset_layout;

	Glib::RefPtr<Pango::Layout> pl    = Glib::wrap((PangoLayout *)para->layout,
                                              /* copy */ true);
	Pango::FontDescription      fd    = pl->get_font_description();
	Cairo::FontSlant            slant = Cairo::FONT_SLANT_NORMAL;
	if (fd.get_style() == Pango::STYLE_OBLIQUE) {
		slant = Cairo::FONT_SLANT_OBLIQUE;
	} else if (fd.get_style() == Pango::STYLE_ITALIC) {
		slant = Cairo::FONT_SLANT_ITALIC;
	}
	Cairo::FontWeight weight = Cairo::FONT_WEIGHT_NORMAL;
	if (fd.get_weight() == Pango::WEIGHT_BOLD) {
		weight = Cairo::FONT_WEIGHT_BOLD;
	}

	double offsetx = 0.0;
	double offsety = 0.0;
	double rotate  = 0.0;

	if ((obj->type == EDGE_OBJTYPE) && (strcmp(para->str, obj->headlabel) == 0)) {
		char *labelrotate = agget(obj->u.e, (char *)"labelrotate");
		if (labelrotate && (strlen(labelrotate) > 0)) {
			rotate = fawkes::deg2rad(atof(labelrotate));
		}
		char *labeloffsetx = agget(obj->u.e, (char *)"labeloffsetx");
		if (labeloffsetx && (strlen(labeloffsetx) > 0)) {
			offsetx = atof(labeloffsetx) * job->scale.x;
		}
		char *labeloffsety = agget(obj->u.e, (char *)"labeloffsety");
		if (labeloffsety && (strlen(labeloffsety) > 0)) {
			offsety = atof(labeloffsety) * job->scale.y;
		}
	}
	//tt_.ping_start(ttc_text_1_);

	Papyrus::Text::pointer t =
	  Papyrus::Text::create(para->str, para->fontsize, fd.get_family(), slant, weight);
	//t->set_stroke(skillgui_render_stroke(&(obj->pencolor)));
	//tt_.ping_end(ttc_text_1_);
	//tt_.ping_start(ttc_text_2_);
#ifdef HAVE_TIMS_PAPYRUS_PATCHES
	t->set_fill(skillgui_render_solidpattern(&(obj->pencolor)), false);
#else
	t->set_fill(skillgui_render_solidpattern(&(obj->pencolor)));
#endif
	//tt_.ping_end(ttc_text_2_);
	//tt_.ping_start(ttc_text_3_);
	t->translate(p.x + offsetx, p.y + offsety, false);
	//tt_.ping_end(ttc_text_3_);
	//tt_.ping_start(ttc_text_4_);
	if (rotate != 0.0)
		t->set_rotation(rotate, Papyrus::RADIANS, false);
	//tt_.ping_end(ttc_text_4_);
	//tt_.ping_start(ttc_text_5_);
	gvp->add_drawable(t);

	//tt_.ping_end(ttc_text_5_);
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_end(ttc_text_);
#endif
}

static void
skillgui_render_ellipse(GVJ_t *job, pointf *A, int filled)
{
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_start(ttc_ellipse_);
	++num_ellipse_;
#endif
	//printf("Render ellipse\n");
	SkillGuiGraphViewport *gvp = (SkillGuiGraphViewport *)job->context;
	obj_state_t           *obj = job->obj;

	double rx = fabs(A[1].x - A[0].x);
	double ry = fabs(A[1].y - A[0].y);

	Papyrus::Circle::pointer e = Papyrus::Circle::create(rx);
	e->set_stroke(skillgui_render_stroke(obj));
	if (filled)
		e->set_fill(skillgui_render_solidpattern(&(obj->fillcolor)));
	e->translate(A[0].x, A[0].y);
	e->set_scale_y(ry / rx);

	gvp->add_drawable(e);
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_end(ttc_ellipse_);
#endif
}

static void
skillgui_render_polygon(GVJ_t *job, pointf *A, int n, int filled)
{
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_start(ttc_polygon_);
	++num_polygon_;
#endif
	//printf("Polygon\n");
	SkillGuiGraphViewport *gvp = (SkillGuiGraphViewport *)job->context;
	obj_state_t           *obj = job->obj;

	Papyrus::Vertices v;
	for (int i = 0; i < n; ++i) {
		v.push_back(Papyrus::Vertex(A[i].x, A[i].y));
	}

	Papyrus::Polygon::pointer p = Papyrus::Polygon::create(v);
	p->set_stroke(skillgui_render_stroke(obj));
	if (filled)
		p->set_fill(skillgui_render_solidpattern(&(obj->fillcolor)));
	gvp->add_drawable(p);
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_end(ttc_polygon_);
#endif
}

static void
skillgui_render_bezier(GVJ_t  *job,
                       pointf *A,
                       int     n,
                       int     arrow_at_start,
                       int     arrow_at_end,
                       int     filled)
{
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_start(ttc_bezier_);
	++num_bezier_;
#endif
	//printf("Bezier\n");
	SkillGuiGraphViewport *gvp = (SkillGuiGraphViewport *)job->context;
	obj_state_t           *obj = job->obj;

	Papyrus::BezierVertices v;
	v.push_back(Papyrus::BezierVertex(A[0].x, A[0].y, A[0].x, A[0].y, A[1].x, A[1].y));
	for (int i = 1; i < n; i += 3) {
		if (i < (n - 4)) {
			v.push_back(Papyrus::BezierVertex(
			  A[i + 2].x, A[i + 2].y, A[i + 1].x, A[i + 1].y, A[i + 3].x, A[i + 3].y));
		} else {
			v.push_back(Papyrus::BezierVertex(
			  A[i + 2].x, A[i + 2].y, A[i + 1].x, A[i + 1].y, A[i + 2].x, A[i + 2].y));
		}
	}

	Papyrus::Bezierline::pointer p = Papyrus::Bezierline::create(v);
	p->set_stroke(skillgui_render_stroke(obj));
	if (filled)
		p->set_fill(skillgui_render_solidpattern(&(obj->fillcolor)));
	gvp->add_drawable(p);
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_end(ttc_bezier_);
#endif
}

static void
skillgui_render_polyline(GVJ_t *job, pointf *A, int n)
{
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_start(ttc_polyline_);
	++num_polyline_;
#endif
	//printf("Polyline\n");
	SkillGuiGraphViewport *gvp = static_cast<SkillGuiGraphViewport *>(job->context);
	obj_state_t           *obj = job->obj;

	Papyrus::Vertices v;
	for (int i = 0; i < n; ++i) {
		v.push_back(Papyrus::Vertex(A[i].x, A[i].y));
	}

	Papyrus::Polyline::pointer p = Papyrus::Polyline::create(v);
	p->set_stroke(skillgui_render_stroke(obj));
	gvp->add_drawable(p);
#ifdef USE_GVPLUGIN_TIMETRACKER
	tt_.ping_end(ttc_polyline_);
#endif
}

static gvrender_engine_t skillgui_render_engine = {
  0, /* skillgui_render_begin_job */
  0, /* skillgui_render_end_job */
  0, /* skillgui_render_begin_graph */
  0, /* skillgui_render_end_graph */
  0, /* skillgui_render_begin_layer */
  0, /* skillgui_render_end_layer */
  skillgui_render_begin_page,
  skillgui_render_end_page,
  0, /* skillgui_render_begin_cluster */
  0, /* skillgui_render_end_cluster */
  0, /* skillgui_render_begin_nodes */
  0, /* skillgui_render_end_nodes */
  0, /* skillgui_render_begin_edges */
  0, /* skillgui_render_end_edges */
  0, /* skillgui_render_begin_node */
  0, /* skillgui_render_end_node */
  0, /* skillgui_render_begin_edge */
  0, /* skillgui_render_end_edge */
  0, /* skillgui_render_begin_anchor */
  0, /* skillgui_render_end_anchor */
  0, /* skillgui_begin_label */
  0, /* skillgui_end_label */
  skillgui_render_textpara,
  0, /* skillgui_render_resolve_color */
  skillgui_render_ellipse,
  skillgui_render_polygon,
  skillgui_render_bezier,
  skillgui_render_polyline,
  0, /* skillgui_render_comment */
  0, /* skillgui_render_library_shape */
};

static gvdevice_engine_t skillgui_device_engine = {
  skillgui_device_init,
  NULL, /* skillgui_device_format */
  skillgui_device_finalize,
};

#ifdef __cplusplus
extern "C" {
#endif

static gvrender_features_t skillgui_render_features = {
  GVRENDER_Y_GOES_DOWN | GVRENDER_DOES_LABELS
    | GVRENDER_DOES_TRANSFORM, /* flags, for Cairo: GVRENDER_DOES_TRANSFORM */
  8,                           /* default pad - graph units */
  0,                           /* knowncolors */
  0,                           /* sizeof knowncolors */
  RGBA_DOUBLE,                 /* color_type */
};

static gvdevice_features_t skillgui_device_features = {
  GVDEVICE_DOES_TRUECOLOR | GVDEVICE_EVENTS, /* flags */
  {0., 0.},                                  /* default margin - points */
  {0., 0.},                                  /* default page width, height - points */
  {96., 96.},                                /* dpi */
};

gvplugin_installed_t gvdevice_types_skillgui[] = {
  {0, (char *)"skillgui:skillgui", 0, &skillgui_device_engine, &skillgui_device_features},
  {0, NULL, 0, NULL, NULL}};

gvplugin_installed_t gvrender_types_skillgui[] = {
  {0, (char *)"skillgui", 10, &skillgui_render_engine, &skillgui_render_features},
  {0, NULL, 0, NULL, NULL}};

static gvplugin_api_t apis[] = {
  {API_device, gvdevice_types_skillgui},
  {API_render, gvrender_types_skillgui},
  {(api_t)0, 0},
};

gvplugin_library_t gvplugin_skillgui_LTX_library = {(char *)"skillgui", apis};

#ifdef __cplusplus
}
#endif

void
gvplugin_skillgui_setup(GVC_t *gvc, SkillGuiGraphViewport *sggvp)
{
	sggvp_ = sggvp;
	gvAddLibrary(gvc, &gvplugin_skillgui_LTX_library);
}
