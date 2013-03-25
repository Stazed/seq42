/*
 * lfownd.h
 *
 *  Created on: 22 mar 2013
 *      Author: mattias
 */

#ifndef LFOWND_H_
#define LFOWND_H_

#include <gtkmm.h>
#include <sigc++/bind.h>
#include "globals.h"
#include "sequence.h"
#include "seqdata.h"

using namespace Gtk;

class lfownd: public Gtk::Window {
public:
	VScale *m_scale_value;
	VScale *m_scale_range;
	VScale *m_scale_speed;
	VScale *m_scale_phase;
	VScale *m_scale_wave;

	double m_value;
	double m_range;
	double m_speed;
	double m_phase;
	int m_wave;

	HBox *m_hbox;
	sequence *m_seq;
	seqdata *m_seqdata;

    void scale_lfo_change();
    static double wave_func(double a_angle, int wave_type);

    public:
    lfownd (sequence *a_seq, seqdata *a_seqdata);
    void toggle_visible();
	virtual ~lfownd();
};

#endif /* LFOWND_H_ */
