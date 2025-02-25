
/***************************************************************************
 *  skillgui.cpp - Skill GUI
 *
 *  Created: Mon Nov 03 13:37:33 2008
 *  Copyright  2008  Tim Niemueller [www.niemueller.de]
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

#include "skillgui.h"
#ifdef USE_PAPYRUS
#	include "graph_viewport.h"
#else
#	include "graph_drawing_area.h"
#endif

#include <blackboard/remote.h>
#include <gui_utils/interface_dispatcher.h>
#include <gui_utils/logview.h>
#include <gui_utils/plugin_tree_view.h>
#include <gui_utils/service_chooser_dialog.h>
#include <netcomm/fawkes/client.h>
#include <utils/system/argparser.h>

#include <cstring>
#include <gvc.h>
#include <iostream>
#include <string>

using namespace fawkes;

#define ACTIVE_SKILL "Active Skill"
#define SKILL_DOT "Skills dot graph"
#define SKILL_SEP_LINE "----------------"

/** @class SkillGuiGtkWindow "skillgui.h"
 * Skill GUI main window.
 * The Skill GUI provides shows Skiller log messages and allows for
 * executing skills.
 * @author Tim Niemueller
 */

/** Constructor.
 * @param cobject C base object
 * @param builder Gtk Builder
 */
SkillGuiGtkWindow::SkillGuiGtkWindow(BaseObjectType                   *cobject,
                                     const Glib::RefPtr<Gtk::Builder> &builder)
: Gtk::Window(cobject)
{
	bb          = NULL;
	skiller_if_ = NULL;
	skdbg_if_   = NULL;
	agdbg_if_   = NULL;

#ifdef HAVE_GCONFMM
	try {
		gconf_ = Gnome::Conf::Client::get_default_client();
		gconf_->add_dir(GCONF_PREFIX);
	} catch (Gnome::Conf::Error &e) {
		std::cerr << e.what() << std::endl;
		throw;
	}
#endif

	builder->get_widget_derived("trv_log", logview_);
	builder->get_widget("tb_connection", tb_connection);
	builder->get_widget("but_clearlog", but_clearlog);
	builder->get_widget("tb_exit", tb_exit);
	builder->get_widget("cbe_skillstring", cbe_skillstring);
	builder->get_widget("but_exec", but_exec);
	builder->get_widget("but_stop", but_stop);
	builder->get_widget("lab_status", lab_status);
	builder->get_widget("lab_alive", lab_alive);
	builder->get_widget("lab_skillstring", lab_skillstring);
	builder->get_widget("lab_error", lab_error);
	builder->get_widget("scw_graph", scw_graph);
	//builder->get_widget("drw_graph", drw_graph);
	builder->get_widget("ntb_tabs", ntb_tabs);
	builder->get_widget("tb_skiller", tb_skiller);
	builder->get_widget("tb_agent", tb_agent);
	builder->get_widget("tb_graphlist", tb_graphlist);
	builder->get_widget("tb_controller", tb_controller);
	builder->get_widget("tb_graphsave", tb_graphsave);
	builder->get_widget("tb_graphopen", tb_graphopen);
	builder->get_widget("tb_graphupd", tb_graphupd);
	builder->get_widget("tb_graphrecord", tb_graphrecord);
	builder->get_widget("tb_zoomin", tb_zoomin);
	builder->get_widget("tb_zoomout", tb_zoomout);
	builder->get_widget("tb_zoomfit", tb_zoomfit);
	builder->get_widget("tb_zoomreset", tb_zoomreset);
	builder->get_widget("tb_graphdir", tb_graphdir);
	builder->get_widget("tb_graphcolored", tb_graphcolored);

#if GTKMM_VERSION_GE(2, 20)
	builder->get_widget("tb_spinner", tb_spinner);
#endif
	builder->get_widget_derived("trv_plugins", trv_plugins_);

	Gtk::SeparatorToolItem *spacesep;
	builder->get_widget("tb_spacesep", spacesep);
	spacesep->set_expand();

	// This should be in the Glade file, but is not restored for some reason
	tb_graphsave->set_homogeneous(false);
	tb_graphopen->set_homogeneous(false);
	tb_graphupd->set_homogeneous(false);
	tb_graphrecord->set_homogeneous(false);
	tb_zoomin->set_homogeneous(false);
	tb_zoomout->set_homogeneous(false);
	tb_zoomfit->set_homogeneous(false);
	tb_zoomreset->set_homogeneous(false);
	tb_graphdir->set_homogeneous(false);
	tb_graphcolored->set_homogeneous(false);

#if GTK_VERSION_GE(3, 0)
	if (!cbe_skillstring->get_has_entry()) {
		throw Exception("Skill string combo box has no entry, invalid UI file?");
	}
#endif
	sks_list_ = Gtk::ListStore::create(sks_record_);
	cbe_skillstring->set_model(sks_list_);
#if GTK_VERSION_GE(3, 0)
	cbe_skillstring->set_entry_text_column(sks_record_.skillstring);
#else
	cbe_skillstring->set_text_column(sks_record_.skillstring);
#endif

	cbe_skillstring->get_entry()->set_activates_default(true);

	trv_plugins_->set_network_client(connection_dispatcher.get_client());
#ifdef HAVE_GCONFMM
	trv_plugins_->set_gconf_prefix(GCONF_PREFIX);
#endif

#ifdef USE_PAPYRUS
	pvp_graph = Gtk::manage(new SkillGuiGraphViewport());
	scw_graph->add(*pvp_graph);
	pvp_graph->show();
#else
	gda = Gtk::manage(new SkillGuiGraphDrawingArea());
	scw_graph->add(*gda);
	gda->show();
#endif

	cb_graphlist = Gtk::manage(new Gtk::ComboBoxText());
#if GTK_VERSION_GE(3, 0)
	cb_graphlist->append(ACTIVE_SKILL);
#else
	cb_graphlist->append_text(ACTIVE_SKILL);
#endif
	cb_graphlist->set_active_text(ACTIVE_SKILL);
	tb_graphlist->add(*cb_graphlist);
	cb_graphlist->show();

	//ntb_tabs->set_current_page(1);

	connection_dispatcher.signal_connected().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_connect));
	connection_dispatcher.signal_disconnected().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_disconnect));

	tb_connection->signal_clicked().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_connection_clicked));
	but_exec->signal_clicked().connect(sigc::mem_fun(*this, &SkillGuiGtkWindow::on_exec_clicked));
	tb_controller->signal_clicked().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_controller_clicked));
	tb_exit->signal_clicked().connect(sigc::mem_fun(*this, &SkillGuiGtkWindow::on_exit_clicked));
	but_stop->signal_clicked().connect(sigc::mem_fun(*this, &SkillGuiGtkWindow::on_stop_clicked));
	but_clearlog->signal_clicked().connect(sigc::mem_fun(*logview_, &LogView::clear));
	tb_skiller->signal_toggled().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_skdbg_data_changed));
	tb_skiller->signal_toggled().connect(
	  sigc::bind(sigc::mem_fun(*cb_graphlist, &Gtk::ComboBoxText::set_sensitive), true));
	tb_agent->signal_toggled().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_agdbg_data_changed));
	tb_agent->signal_toggled().connect(
	  sigc::bind(sigc::mem_fun(*cb_graphlist, &Gtk::ComboBoxText::set_sensitive), false));
	cb_graphlist->signal_changed().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_skill_changed));
	tb_graphupd->signal_clicked().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_graphupd_clicked));
	tb_graphdir->signal_clicked().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_graphdir_clicked));
	tb_graphcolored->signal_toggled().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_graphcolor_toggled));
#ifdef USE_PAPYRUS
	tb_graphsave->signal_clicked().connect(sigc::mem_fun(*pvp_graph, &SkillGuiGraphViewport::save));
	tb_zoomin->signal_clicked().connect(sigc::mem_fun(*pvp_graph, &SkillGuiGraphViewport::zoom_in));
	tb_zoomout->signal_clicked().connect(sigc::mem_fun(*pvp_graph, &SkillGuiGraphViewport::zoom_out));
	tb_zoomfit->signal_clicked().connect(sigc::mem_fun(*pvp_graph, &SkillGuiGraphViewport::zoom_fit));
	tb_zoomreset->signal_clicked().connect(
	  sigc::mem_fun(*pvp_graph, &SkillGuiGraphViewport::zoom_reset));
#else
	tb_graphsave->signal_clicked().connect(sigc::mem_fun(*gda, &SkillGuiGraphDrawingArea::save));
	tb_graphopen->signal_clicked().connect(sigc::mem_fun(*gda, &SkillGuiGraphDrawingArea::open));
	tb_zoomin->signal_clicked().connect(sigc::mem_fun(*gda, &SkillGuiGraphDrawingArea::zoom_in));
	tb_zoomout->signal_clicked().connect(sigc::mem_fun(*gda, &SkillGuiGraphDrawingArea::zoom_out));
	tb_zoomfit->signal_clicked().connect(sigc::mem_fun(*gda, &SkillGuiGraphDrawingArea::zoom_fit));
	tb_zoomreset->signal_clicked().connect(
	  sigc::mem_fun(*gda, &SkillGuiGraphDrawingArea::zoom_reset));
	tb_graphrecord->signal_clicked().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_recording_toggled));
	gda->signal_update_disabled().connect(
	  sigc::mem_fun(*this, &SkillGuiGtkWindow::on_update_disabled));
#endif

#ifdef HAVE_GCONFMM
	gconf_->signal_value_changed().connect(
	  sigc::hide(sigc::hide(sigc::mem_fun(*this, &SkillGuiGtkWindow::on_config_changed))));
	on_config_changed();
#endif
}

/** Destructor. */
SkillGuiGtkWindow::~SkillGuiGtkWindow()
{
#ifdef HAVE_GCONFMM
	gconf_->remove_dir(GCONF_PREFIX);
#endif
	logview_->set_client(NULL);
	trv_plugins_->set_network_client(NULL);
}

void
SkillGuiGtkWindow::on_config_changed()
{
#ifdef HAVE_GCONFMM
	Gnome::Conf::SListHandle_ValueString l(gconf_->get_string_list(GCONF_PREFIX "/command_history"));

	sks_list_->clear();
	for (Gnome::Conf::SListHandle_ValueString::const_iterator i = l.begin(); i != l.end(); ++i) {
		Gtk::TreeModel::Row row      = *sks_list_->append();
		row[sks_record_.skillstring] = *i;
	}

	bool colored = gconf_->get_bool(GCONF_PREFIX "/graph_colored");
	tb_graphcolored->set_active(colored);
#endif
}

void
SkillGuiGtkWindow::on_skill_changed()
{
	Glib::ustring skill = cb_graphlist->get_active_text();
	if (skill == ACTIVE_SKILL || skill == SKILL_SEP_LINE) {
		skill = "ACTIVE";
	} else if (skill == SKILL_DOT) {
		skill = "SKILL_DEP";
	}
	SkillerDebugInterface::SetGraphMessage *sgm =
	  new SkillerDebugInterface::SetGraphMessage(skill.c_str());
	skdbg_if_->msgq_enqueue(sgm);
}

/** Event handler for connection button. */
void
SkillGuiGtkWindow::on_connection_clicked()
{
	if (!connection_dispatcher.get_client()->connected()) {
		ServiceChooserDialog ssd(*this, connection_dispatcher.get_client());
		ssd.run_and_connect();
	} else {
		connection_dispatcher.get_client()->disconnect();
	}
}

void
SkillGuiGtkWindow::on_exit_clicked()
{
	Gtk::Main::quit();
}

void
SkillGuiGtkWindow::on_controller_clicked()
{
	if (skiller_if_ && skiller_if_->is_valid() && skiller_if_->has_writer()
	    && skiller_if_->exclusive_controller() == skiller_if_->serial().get_string()) {
		// we are exclusive controller, release control
		SkillerInterface::ReleaseControlMessage *rcm = new SkillerInterface::ReleaseControlMessage();
		skiller_if_->msgq_enqueue(rcm);
	} else if (skiller_if_ && skiller_if_->is_valid() && skiller_if_->has_writer()
	           && strcmp(skiller_if_->exclusive_controller(), "") == 0) {
		// there is no exclusive controller, try to acquire control
		SkillerInterface::AcquireControlMessage *acm = new SkillerInterface::AcquireControlMessage();
		skiller_if_->msgq_enqueue(acm);
	} else {
		Gtk::MessageDialog md(*this,
		                      "Another component already acquired the exclusive "
		                      "control for the Skiller; not acquiring exclusive control.",
		                      /* markup */ false,
		                      Gtk::MESSAGE_ERROR,
		                      Gtk::BUTTONS_OK,
		                      /* modal */ true);
		md.set_title("Control Acquisition Failed");
		md.run();
	}
}

void
SkillGuiGtkWindow::on_stop_clicked()
{
	if (bb && skiller_if_ && skiller_if_->is_valid() && skiller_if_->has_writer()) {
		SkillerInterface::StopExecMessage *sem = new SkillerInterface::StopExecMessage();
		skiller_if_->msgq_enqueue(sem);
	}
}

void
SkillGuiGtkWindow::close_bb()
{
	if (bb) {
		bb->unregister_listener(skiller_ifd_);
		bb->unregister_listener(skdbg_ifd_);
		bb->unregister_listener(agdbg_ifd_);
		delete skiller_ifd_;
		delete skdbg_ifd_;
		delete agdbg_ifd_;
		if (skiller_if_ && skiller_if_->is_valid() && skiller_if_->has_writer()
		    && skiller_if_->exclusive_controller() == skiller_if_->serial().get_string()) {
			SkillerInterface::ReleaseControlMessage *rcm = new SkillerInterface::ReleaseControlMessage();
			skiller_if_->msgq_enqueue(rcm);
		}
		bb->close(skiller_if_);
		bb->close(skdbg_if_);
		bb->close(agdbg_if_);
		delete bb;
		skiller_if_ = NULL;
		skdbg_if_   = NULL;
		agdbg_if_   = NULL;
		bb          = NULL;
	}
}

/** Event handler for connected event. */
void
SkillGuiGtkWindow::on_connect()
{
	try {
		if (!bb) {
			bb          = new RemoteBlackBoard(connection_dispatcher.get_client());
			skiller_if_ = bb->open_for_reading<SkillerInterface>("Skiller");
			skdbg_if_   = bb->open_for_reading<SkillerDebugInterface>("Skiller");
			agdbg_if_   = bb->open_for_reading<SkillerDebugInterface>("LuaAgent");
			on_skiller_data_changed();
			on_skdbg_data_changed();
			on_agdbg_data_changed();

			skiller_ifd_ = new InterfaceDispatcher("Skiller IFD", skiller_if_);
			skdbg_ifd_   = new InterfaceDispatcher("SkillerDebug IFD", skdbg_if_);
			agdbg_ifd_   = new InterfaceDispatcher("LuaAgent SkillerDebug IFD", agdbg_if_);
			bb->register_listener(skiller_ifd_, BlackBoard::BBIL_FLAG_DATA);
			bb->register_listener(skdbg_ifd_, BlackBoard::BBIL_FLAG_DATA);
			bb->register_listener(agdbg_ifd_, BlackBoard::BBIL_FLAG_DATA);
			skiller_ifd_->signal_data_changed().connect(
			  sigc::hide(sigc::mem_fun(*this, &SkillGuiGtkWindow::on_skiller_data_changed)));
			skdbg_ifd_->signal_data_changed().connect(
			  sigc::hide(sigc::mem_fun(*this, &SkillGuiGtkWindow::on_skdbg_data_changed)));
			agdbg_ifd_->signal_data_changed().connect(
			  sigc::hide(sigc::mem_fun(*this, &SkillGuiGtkWindow::on_agdbg_data_changed)));

			// always try to acquire control on connect, this may well fail, for
			// example if agent is running, but we don't care
			skiller_if_->read();
			if (skiller_if_->has_writer() && strcmp(skiller_if_->exclusive_controller(), "") == 0) {
				SkillerInterface::AcquireControlMessage *aqm =
				  new SkillerInterface::AcquireControlMessage();
				skiller_if_->msgq_enqueue(aqm);
			}
			if (skdbg_if_->has_writer()) {
				SkillerDebugInterface::SetGraphMessage *sgm =
				  new SkillerDebugInterface::SetGraphMessage("LIST");
				skdbg_if_->msgq_enqueue(sgm);
			}
		}
		tb_connection->set_stock_id(Gtk::Stock::DISCONNECT);
		logview_->set_client(connection_dispatcher.get_client());

		tb_controller->set_sensitive(true);
		cbe_skillstring->set_sensitive(true);

		this->set_title(std::string("Skill GUI @ ")
		                + connection_dispatcher.get_client()->get_hostname());
	} catch (Exception &e) {
		Glib::ustring      message = *(e.begin());
		Gtk::MessageDialog md(*this,
		                      message,
		                      /* markup */ false,
		                      Gtk::MESSAGE_ERROR,
		                      Gtk::BUTTONS_OK,
		                      /* modal */ true);
		md.set_title("BlackBoard connection failed");
		md.run();

		close_bb();
		connection_dispatcher.get_client()->disconnect();
	}
}

/** Event handler for disconnected event. */
void
SkillGuiGtkWindow::on_disconnect()
{
	tb_controller->set_sensitive(false);
	cbe_skillstring->set_sensitive(false);
	but_exec->set_sensitive(false);
	but_stop->set_sensitive(false);

	close_bb();

	tb_connection->set_stock_id(Gtk::Stock::CONNECT);
#ifdef USE_PAPYRUS
	pvp_graph->queue_draw();
#endif
	logview_->set_client(NULL);

	this->set_title("Skill GUI");
}

void
SkillGuiGtkWindow::on_exec_clicked()
{
	Glib::ustring sks = "";
	if (cbe_skillstring->get_active_row_number() == -1) {
		Gtk::Entry *entry = cbe_skillstring->get_entry();
		sks               = entry->get_text();
	} else {
		Gtk::TreeModel::Row row = *cbe_skillstring->get_active();
#if GTK_VERSION_GE(3, 0)
		row.get_value(cbe_skillstring->get_entry_text_column(), sks);
#else
		row.get_value(cbe_skillstring->get_text_column(), sks);
#endif
	}

	if (sks != "") {
#if GTKMM_VERSION_GE(2, 20)
		tb_spinner->start();
#endif

		if (skiller_if_ && skiller_if_->is_valid() && skiller_if_->has_writer()
		    && skiller_if_->exclusive_controller() == skiller_if_->serial().get_string()) {
			SkillerInterface::ExecSkillMessage *esm = new SkillerInterface::ExecSkillMessage(sks.c_str());
			skiller_if_->msgq_enqueue(esm);

			Gtk::TreeModel::Children children = sks_list_->children();
			bool                     ok       = true;
			if (!children.empty()) {
				size_t        num = 0;
				Gtk::TreeIter i   = children.begin();
				while (ok && (i != children.end())) {
					if (num >= 9) {
						i = sks_list_->erase(i);
					} else {
						Gtk::TreeModel::Row row = *i;
						ok                      = (row[sks_record_.skillstring] != sks);
						++num;
						++i;
					}
				}
			}
			if (ok) {
				Gtk::TreeModel::Row row      = *sks_list_->prepend();
				row[sks_record_.skillstring] = sks;

				std::list<Glib::ustring> l;
				for (Gtk::TreeIter i = children.begin(); i != children.end(); ++i) {
					Gtk::TreeModel::Row row = *i;
					l.push_back(row[sks_record_.skillstring]);
				}

#ifdef HAVE_GCONFMM
				gconf_->set_string_list(GCONF_PREFIX "/command_history", l);
#endif
			}
		} else {
			Gtk::MessageDialog md(*this,
			                      "The exclusive control over the skiller has "
			                      "not been acquired yet and skills cannot be executed",
			                      /* markup */ false,
			                      Gtk::MESSAGE_ERROR,
			                      Gtk::BUTTONS_OK,
			                      /* modal */ true);
			md.set_title("Skill Execution Failure");
			md.run();
		}
	}
}

void
SkillGuiGtkWindow::on_skiller_data_changed()
{
	try {
		skiller_if_->read();

		switch (skiller_if_->status()) {
		case SkillerInterface::S_INACTIVE:
#if GTKMM_VERSION_GE(2, 20)
			tb_spinner->stop();
#endif
			lab_status->set_text("S_INACTIVE");
			break;
		case SkillerInterface::S_FINAL:
#if GTKMM_VERSION_GE(2, 20)
			tb_spinner->stop();
#endif
			//throbber_->set_stock(Gtk::Stock::APPLY);
			lab_status->set_text("S_FINAL");
			break;
		case SkillerInterface::S_RUNNING:
#if GTKMM_VERSION_GE(2, 20)
			tb_spinner->start();
#endif
			lab_status->set_text("S_RUNNING");
			break;
		case SkillerInterface::S_FAILED:
#if GTKMM_VERSION_GE(2, 20)
			tb_spinner->stop();
#endif
			//throbber_->set_stock(Gtk::Stock::DIALOG_WARNING);
			lab_status->set_text("S_FAILED");
			break;
		}

		lab_skillstring->set_text(skiller_if_->skill_string());
		lab_error->set_text(skiller_if_->error());
#if GTKMM_MAJOR_VERSION > 2 || (GTKMM_MAJOR_VERSION == 2 && GTKMM_MINOR_VERSION >= 12)
		lab_skillstring->set_tooltip_text(skiller_if_->skill_string());
		lab_error->set_tooltip_text(skiller_if_->error());
#endif
		lab_alive->set_text(skiller_if_->has_writer() ? "Yes" : "No");

		if (skiller_if_->exclusive_controller() == skiller_if_->serial().get_string()) {
			if (tb_controller->get_stock_id() == Gtk::Stock::NO.id) {
				tb_controller->set_stock_id(Gtk::Stock::YES);
#if GTKMM_MAJOR_VERSION > 2 || (GTKMM_MAJOR_VERSION == 2 && GTKMM_MINOR_VERSION >= 12)
				tb_controller->set_tooltip_text("Release exclusive control");
#endif
			}
			but_exec->set_sensitive(true);
			but_stop->set_sensitive(true);
		} else {
			if (tb_controller->get_stock_id() == Gtk::Stock::YES.id) {
				tb_controller->set_stock_id(Gtk::Stock::NO);
#if GTKMM_MAJOR_VERSION > 2 || (GTKMM_MAJOR_VERSION == 2 && GTKMM_MINOR_VERSION >= 12)
				tb_controller->set_tooltip_text("Gain exclusive control");
#endif
			}
			but_exec->set_sensitive(false);
			but_stop->set_sensitive(false);
		}

	} catch (Exception &e) {
#if GTKMM_VERSION_GE(2, 20)
		tb_spinner->stop();
#endif
	}
}

void
SkillGuiGtkWindow::on_skdbg_data_changed()
{
	if (tb_skiller->get_active() && skdbg_if_) {
		try {
			skdbg_if_->read();

			if (strcmp(skdbg_if_->graph_fsm(), "LIST") == 0) {
				Glib::ustring list = skdbg_if_->graph();
#if GTK_VERSION_GE(3, 0)
				cb_graphlist->remove_all();
				cb_graphlist->append(ACTIVE_SKILL);
				cb_graphlist->append(SKILL_DOT);
				cb_graphlist->append(SKILL_SEP_LINE);
#else
				cb_graphlist->clear_items();
				cb_graphlist->append_text(ACTIVE_SKILL);
				cb_graphlist->append_text(SKILL_DOT);
				cb_graphlist->append_text(SKILL_SEP_LINE);
#endif
				cb_graphlist->set_active_text(ACTIVE_SKILL);
#if GTK_VERSION_GE(2, 14)
				Glib::RefPtr<Glib::Regex> regex  = Glib::Regex::create("\n");
				std::list<std::string>    skills = regex->split(list);
				for (std::list<std::string>::iterator i = skills.begin(); i != skills.end(); ++i) {
#	if GTK_VERSION_GE(3, 0)
					if (*i != "")
						cb_graphlist->append(*i);
#	else
					if (*i != "")
						cb_graphlist->append_text(*i);
#	endif
				}
#endif
				if (skdbg_if_->has_writer()) {
					SkillerDebugInterface::SetGraphMessage *sgm =
					  new SkillerDebugInterface::SetGraphMessage("ACTIVE");
					skdbg_if_->msgq_enqueue(sgm);
				}
			} else {
#ifdef USE_PAPYRUS
				pvp_graph->set_graph_fsm(skdbg_if_->graph_fsm());
				pvp_graph->set_graph(skdbg_if_->graph());
				pvp_graph->render();
#else
				gda->set_graph_fsm(skdbg_if_->graph_fsm());
				gda->set_graph(skdbg_if_->graph());
#endif
			}

			switch (skdbg_if_->graph_dir()) {
			case SkillerDebugInterface::GD_TOP_BOTTOM:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_DOWN);
				break;
			case SkillerDebugInterface::GD_BOTTOM_TOP:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_UP);
				break;
			case SkillerDebugInterface::GD_LEFT_RIGHT:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_FORWARD);
				break;
			case SkillerDebugInterface::GD_RIGHT_LEFT:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_BACK);
				break;
			}

			if (skdbg_if_->is_graph_colored() != tb_graphcolored->get_active()) {
				tb_graphcolored->set_active(skdbg_if_->is_graph_colored());
			}
		} catch (Exception &e) {
			// ignored
		}
	}
}

void
SkillGuiGtkWindow::on_agdbg_data_changed()
{
	if (tb_agent->get_active() && agdbg_if_) {
		try {
			agdbg_if_->read();
#ifdef USE_PAPYRUS
			pvp_graph->set_graph_fsm(agdbg_if_->graph_fsm());
			pvp_graph->set_graph(agdbg_if_->graph());
			pvp_graph->render();
#else
			gda->set_graph_fsm(agdbg_if_->graph_fsm());
			gda->set_graph(agdbg_if_->graph());
#endif

			switch (agdbg_if_->graph_dir()) {
			case SkillerDebugInterface::GD_TOP_BOTTOM:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_DOWN);
				break;
			case SkillerDebugInterface::GD_BOTTOM_TOP:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_UP);
				break;
			case SkillerDebugInterface::GD_LEFT_RIGHT:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_FORWARD);
				break;
			case SkillerDebugInterface::GD_RIGHT_LEFT:
				tb_graphdir->set_stock_id(Gtk::Stock::GO_BACK);
				break;
			}
		} catch (Exception &e) {
			// ignored
		}
	}
}

void
SkillGuiGtkWindow::on_graphupd_clicked()
{
#ifdef USE_PAPYRUS
	if (pvp_graph->get_update_graph()) {
		pvp_graph->set_update_graph(false);
		tb_graphupd->set_stock_id(Gtk::Stock::MEDIA_STOP);
	} else {
		pvp_graph->set_update_graph(true);
		tb_graphupd->set_stock_id(Gtk::Stock::MEDIA_PLAY);
		pvp_graph->render();
	}
#else
	if (gda->get_update_graph()) {
		gda->set_update_graph(false);
		tb_graphupd->set_stock_id(Gtk::Stock::MEDIA_STOP);
	} else {
		gda->set_update_graph(true);
		tb_graphupd->set_stock_id(Gtk::Stock::MEDIA_PLAY);
	}
#endif
}

void
SkillGuiGtkWindow::on_graphdir_clicked()
{
	SkillerDebugInterface *iface = skdbg_if_;
	if (tb_agent->get_active()) {
		iface = agdbg_if_;
	}

	Glib::ustring stockid = tb_graphdir->get_stock_id();
	if (stockid == Gtk::Stock::GO_DOWN.id) {
		send_graphdir_message(iface, SkillerDebugInterface::GD_BOTTOM_TOP);
	} else if (stockid == Gtk::Stock::GO_UP.id) {
		send_graphdir_message(iface, SkillerDebugInterface::GD_LEFT_RIGHT);
	} else if (stockid == Gtk::Stock::GO_FORWARD.id) {
		send_graphdir_message(iface, SkillerDebugInterface::GD_RIGHT_LEFT);
	} else if (stockid == Gtk::Stock::GO_BACK.id) {
		send_graphdir_message(iface, SkillerDebugInterface::GD_TOP_BOTTOM);
	}
}

void
SkillGuiGtkWindow::send_graphdir_message(SkillerDebugInterface                    *iface,
                                         SkillerDebugInterface::GraphDirectionEnum gd)
{
	try {
		if (iface) {
			SkillerDebugInterface::SetGraphDirectionMessage *m;
			m = new SkillerDebugInterface::SetGraphDirectionMessage(gd);
			iface->msgq_enqueue(m);
		} else {
			throw Exception("Not connected to Fawkes.");
		}
	} catch (Exception &e) {
		Gtk::MessageDialog md(*this,
		                      Glib::ustring("Setting graph direction failed: ") + e.what(),
		                      /* markup */ false,
		                      Gtk::MESSAGE_ERROR,
		                      Gtk::BUTTONS_OK,
		                      /* modal */ true);
		md.set_title("Communication Failure");
		md.run();
	}
}

void
SkillGuiGtkWindow::on_graphdir_changed(SkillerDebugInterface::GraphDirectionEnum gd)
{
	if (tb_agent->get_active()) {
		send_graphdir_message(agdbg_if_, gd);
	} else {
		send_graphdir_message(skdbg_if_, gd);
	}
}

void
SkillGuiGtkWindow::on_graphcolor_toggled()
{
#ifdef HAVE_GCONFMM
	gconf_->set(GCONF_PREFIX "/graph_colored", tb_graphcolored->get_active());
#endif

	SkillerDebugInterface *iface = skdbg_if_;
	if (tb_agent->get_active()) {
		iface = agdbg_if_;
	}

	try {
		if (iface) {
			SkillerDebugInterface::SetGraphColoredMessage *m;
			m = new SkillerDebugInterface::SetGraphColoredMessage(tb_graphcolored->get_active());
			iface->msgq_enqueue(m);
		} else {
			throw Exception("Not connected to Fawkes.");
		}
	} catch (Exception &e) {
		/* Ignore for now, causes error message on startup
    Gtk::MessageDialog md(*this,
			  Glib::ustring("Setting graph color failed: ") + e.what(),
			  / markup / false,
			  Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
			  / modal / true);
    md.set_title("Communication Failure");
    md.run();
    */
	}
}

/** Constructor. */
SkillGuiGtkWindow::SkillStringRecord::SkillStringRecord()
{
	add(skillstring);
}

void
SkillGuiGtkWindow::on_update_disabled()
{
#ifdef USE_PAPYRUS
#else
	tb_graphupd->set_stock_id(Gtk::Stock::MEDIA_STOP);
#endif
}

void
SkillGuiGtkWindow::on_recording_toggled()
{
#ifdef USE_PAPYRUS
#else
	bool active = tb_graphrecord->get_active();
	if (gda->set_recording(active) != active) {
		tb_graphrecord->set_active(!active);
	}
#endif
}
