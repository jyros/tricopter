/*
 * RC.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: jairo
 */

#include "RC.h"

RC::RC() {

	int i;

	// initialize class members
	for(i=0;i<9;i++) _ppm_sum[i]=0;
	_sp = -1;
	_sync_error = ERROR_SYNC;

}

void RC::init() {

	// ppm decode setup
	init_ppm_timer_and_dma();
	ppm_decode_go();

}

void RC::store_ppm_sum() {

}

void RC::update() {

	// read ppm sum data
	rx_read(&_sp, (float *)&_rcCmd);

#if VERBOSITY>3
	SerialUSB.print("RC::update(): _sp:");
	SerialUSB.println(_sp);
#endif

	if(_sp == SP_INVALID)
	{
		_throttle=0.0;
		return;
	}

	_yaw = _rcCmd[3];
	_pitch = _rcCmd[1];
	_roll = _rcCmd[0];
	_throttle = _rcCmd[2];
	_aux1 = _rcCmd[4];
	_aux2 = _rcCmd[5];
	_aux3 = _rcCmd[6];
	_aux4 = _rcCmd[7];

}

uint16 RC::isOK() {
	return ! status();
}

uint16 RC::status() {

	if(_sp == SP_INVALID)
		return ERROR_SYNC;
	else
		return SUCCESS;

}

float RC::get_channel(int channel) {
	if(_sp == SP_INVALID)
		return CH_INVALID;

	switch ( channel ) {

	case CH_ROLL:
		return _roll;

	case CH_PITCH:
		return _pitch;

	case CH_THROTTLE:
		return _throttle;

	case CH_YAW:
		return _yaw;

	case CH_AUX1:
		return _aux1;

	case CH_AUX2:
		return _aux2;

	case CH_AUX3:
		return _aux3;

	case CH_AUX4:
		return _aux4;
	}

	return -999.9;
}


int RC::get_sync_pulse()
{
	return _sp;
}
