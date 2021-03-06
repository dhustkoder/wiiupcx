#include <stdlib.h>
#include <vpad/input.h>
#include <string.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <coreinit/thread.h>
#include <stdint.h>
#include "packets.h"
#include "connection.h"
#include "input.h"
#include "log.h"


static VPADReadError verror;
static VPADStatus vpad;
static VPADStatus last_vpad;
static KPADStatus kpads[4];
static volatile uint8_t kpad_connected[4] = { 0, 0, 0, 0 };
static volatile uint8_t wiimote_flags = 0;
static struct input_packet last_input;
static uint8_t rumble_pattern[120];


static void wpad_connection_callback(WPADChan chan, int32_t status)
{
	log_debug(
		"UPDATED WPAD[%d] = STATUS %d",
		(int)chan,
		(int)status
	);

	WLU_ASSERT(chan >= 0 && chan <= 3);
	kpad_connected[chan] = status == 0 ? 1 : 0;
	
	int kchan_flags[4] = {
		[WPAD_CHAN_0] = INPUT_PACKET_FLAG_WIIMOTE_0,
		[WPAD_CHAN_1] = INPUT_PACKET_FLAG_WIIMOTE_1,
		[WPAD_CHAN_2] = INPUT_PACKET_FLAG_WIIMOTE_2,
		[WPAD_CHAN_3] = INPUT_PACKET_FLAG_WIIMOTE_3
	};

	if (!kpad_connected[chan]) {
		memset(&kpads[chan], 0, sizeof kpads[chan]);
		wiimote_flags &= ~kchan_flags[chan];
	} else {
		wiimote_flags |= kchan_flags[chan];
	}
}






void input_init(void)
{
	VPADInit();
	WPADInit();
	WPADEnableURCC(1);

	WPADSetConnectCallback(WPAD_CHAN_0, wpad_connection_callback);
	WPADSetConnectCallback(WPAD_CHAN_1, wpad_connection_callback);
	WPADSetConnectCallback(WPAD_CHAN_2, wpad_connection_callback);
	WPADSetConnectCallback(WPAD_CHAN_3, wpad_connection_callback);

	memset(kpads, 0, sizeof kpads);
	memset(&vpad, 0, sizeof vpad);
	memset(rumble_pattern, 0xFF, sizeof rumble_pattern);
}

void input_term(void)
{
	WPADShutdown();
	VPADShutdown();
}

bool input_update(struct input_packet* input)
{	
	VPADRead(VPAD_CHAN_0, &vpad, 1, &verror);
	VPADGetTPCalibratedPoint(VPAD_CHAN_0, &vpad.tpNormal, &vpad.tpNormal);
	
	if (verror != VPAD_READ_SUCCESS) {
	
		if (verror != VPAD_READ_NO_SAMPLES) {
			log_debug("VPADRead failed, reason: %d", verror);
		}
	}

	memcpy(&last_vpad, &vpad, sizeof(VPADStatus));

	for (int i = 0; i < 4; ++i) {
		if (kpad_connected[i]) {
			WPADRead(i, &kpads[i]);
		}
	}

	last_input.flags = wiimote_flags|INPUT_PACKET_FLAG_GAMEPAD;
	const VPADVec2D ls = vpad.leftStick;
	const VPADVec2D rs = vpad.rightStick;
	
	last_input.gamepad.btns = vpad.hold;
	last_input.gamepad.lsx = ls.x * INT16_MAX;
	last_input.gamepad.lsy = ls.y * INT16_MAX;
	last_input.gamepad.rsx = rs.x * INT16_MAX;
	last_input.gamepad.rsy = rs.y * INT16_MAX;

	for (int i = 0; i < 4; ++i) {
		if (kpad_connected[i]) {
			last_input.wiimotes[i].btns = kpads[i].hold;
		}
	}

	
	return input_fetch(input);
}

void input_update_feedback(const struct input_feedback_packet* fb)
{
	static bool is_rumbling = false;
	if (fb->placeholder != 0x00) {
		if (!is_rumbling) {
			is_rumbling = true;
			VPADControlMotor(VPAD_CHAN_0, rumble_pattern, sizeof rumble_pattern);
		}
	} else {
		if (is_rumbling) {
			is_rumbling = false;
			VPADStopMotor(VPAD_CHAN_0);
		}
	}
}

bool input_fetch(struct input_packet* input)
{
	if (memcmp(input, &last_input, sizeof *input) != 0) {
		memcpy(input, &last_input, sizeof *input);
		return true;
	}
	return false;
}


void input_fetch_vpad(VPADStatus* target)
{
	uint32_t trigger = target->hold&(last_vpad.hold^target->hold);
	memcpy(target, &last_vpad, sizeof(VPADStatus));
	target->trigger = trigger;
}




