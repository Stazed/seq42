/*
 * lfownd.cpp
 *
 *  Created on: 22 mar 2013
 *      Author: mattias
 */

#include "lfownd.h"
#include <string>
#include <math.h>
#include <sigc++/slot.h>
#include "seqedit.h"
using std::string;
using sigc::mem_fun;

#define PI (3.14159265359)


double lfownd::wave_func(double a_angle, int wave_type) {
	double tmp;
	switch (wave_type){
	case 1:
		return sin(a_angle * PI * 2.);
	case 2:
		return (a_angle - (int)(a_angle)) * 2. - 1.;
	case 3:
		return (a_angle - (int)(a_angle)) * -2. + 1.;
	case 4:
		tmp = (a_angle * 2. - (int)(a_angle * 2.));
		if (((int)(a_angle * 2.)) % 2 == 1){
			tmp = -tmp + 1.;
		}
		tmp = tmp * 2. - 1.;
		return tmp;
	default:
		return 0;
	}
}

lfownd::~lfownd() {
	// TODO Auto-generated destructor stub
}


lfownd::lfownd(sequence *a_seq, seqdata *a_seqdata){
	m_seq = a_seq;
	m_seqdata = a_seqdata;
    /* main window */
    string title = "seq42 - lfoeditor - ";
    title.append(m_seq->get_name());
    set_title(title);
    set_size_request(150, 200);

	m_scale_value = manage(new VScale(0, 127, .1));
	m_scale_range = manage(new VScale(0, 127, .1));
	m_scale_speed = manage(new VScale(0, 16, .01));
	m_scale_phase = manage(new VScale(0,1,.01));
	m_scale_wave = manage(new VScale(1,5,1));

	m_scale_value->set_tooltip_text("value");
	m_scale_range->set_tooltip_text("range");
	m_scale_speed->set_tooltip_text("speed");
	m_scale_phase->set_tooltip_text("phase");
	m_scale_wave->set_tooltip_text("wave");

	m_scale_value->set_value(64);
	m_scale_range->set_value(64);
	m_scale_value->signal_value_changed().connect(mem_fun( *this, &lfownd::scale_lfo_change));
	m_scale_range->signal_value_changed().connect(mem_fun( *this, &lfownd::scale_lfo_change));
	m_scale_speed->signal_value_changed().connect(mem_fun( *this, &lfownd::scale_lfo_change));
	m_scale_phase->signal_value_changed().connect(mem_fun( *this, &lfownd::scale_lfo_change));
	m_scale_wave->signal_value_changed().connect(mem_fun( *this, &lfownd::scale_lfo_change));

	m_hbox = manage(new HBox(false, 2));

	add(*m_hbox);
	m_hbox->pack_start(*m_scale_value);
	m_hbox->pack_start(*m_scale_range);
	m_hbox->pack_start(*m_scale_speed);
	m_hbox->pack_start(*m_scale_phase);
	m_hbox->pack_start(*m_scale_wave);
}

void lfownd::toggle_visible(){
	show_all();
}

void lfownd::scale_lfo_change() {
	m_value = m_scale_value->get_value();
	m_range = m_scale_range->get_value();
	m_speed = m_scale_speed->get_value();
	m_phase = m_scale_phase->get_value();
	m_wave = m_scale_wave->get_value();
	m_seq->change_event_data_lfo(m_value, m_range, m_speed, m_phase, m_wave,
			m_seqdata->m_status, m_seqdata->m_cc);
	m_seqdata->update_pixmap();
	m_seqdata->draw_pixmap_on_window();
}

