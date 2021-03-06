#include <wut_types.h>
#include <whb/proc.h>
#include <whb/libmanager.h>
#include <coreinit/thread.h>
#include <stdlib.h>
#include <string.h>
#include <whb/log.h>
#include <whb/log_udp.h>
#include "connection.h"
#include "input.h"
#include "gui.h"
#include "utils.h"
#include "log.h"


static volatile bool terminate_threads = false;
static volatile const char* entered_ip = NULL;


static int connection_state_manager_thread(
	__attribute__((unused)) int  a,
	__attribute__((unused)) const char** b
)
{
	while (!terminate_threads) {

		if (connection_is_connected()) {
			
			OSSleepTicks(OSSecondsToTicks(PING_INTERVAL_SEC));

			if (!connection_ping_host()) {
				log_info("disconnected");
			}

		} else {

			if (entered_ip != NULL) {
				log_info("trying to connect...");
				if (connection_connect((char*)entered_ip)) {
					log_info("connected");
				} else {
					log_info("failed to connect");
					entered_ip = NULL;
				}
			}

			OSSleepTicks(OSMillisecondsToTicks(1000));

		}
	}

	return 0;
}


static int input_manager_thread(
	__attribute__((unused)) int  a,
	__attribute__((unused)) const char** b
)
{
	struct input_feedback_packet feedback;
	struct input_packet input;

	while (!terminate_threads) {
		if (input_update(&input))
			connection_send_input_packet(&input);
		if (connection_receive_input_feedback_packet(&feedback))
			input_update_feedback(&feedback);

		OSSleepTicks(OSMillisecondsToTicks(4));
	}

	return 0;
}


static bool platform_init(void)
{
	WHBProcInit();
	
	if (!log_init())
		return false;

	input_init();

	if (!gui_init())
		return false;

	if (!connection_init())
		return false;

	OSRunThread(
		OSGetDefaultThread(0),
		input_manager_thread,
		0,
		NULL
	);

	OSRunThread(
		OSGetDefaultThread(2),
		connection_state_manager_thread,
		0,
		NULL
	);

	return true;
}


static void platform_term(void)
{
	terminate_threads = true;

	connection_term();
	
	gui_term();

	input_term();

	log_term();
	WHBProcShutdown();
}


int main(void)
{
	if (!platform_init())
		return EXIT_FAILURE;

	//stream_decoder_init();
	while (WHBProcIsRunning()) {

		//stream_update();
		// OSTick t = OSGetSystemTick();
		if (!gui_update(&entered_ip)) {
			break;
		}

		// log_debug("%d MS PER FRAME", OSTicksToMilliseconds(OSGetSystemTick() - t));

	}

	//stream_decoder_term();
	platform_term();

	return EXIT_SUCCESS;
}
