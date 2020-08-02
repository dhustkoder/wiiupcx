#ifndef WIIUPCX_CONNECTION_H_
#define WIIUPCX_CONNECTION_H_
#include "packets.h"


extern bool connection_init(void);
extern void connection_term(void);
extern const char* connection_get_host_address(void);
extern const char* connection_get_client_address(void);
extern bool connection_is_connected(void);

extern bool connection_receive_input_packet(struct input_packet* input);
extern void connection_send_input_feedback_packet(const struct input_feedback_packet* feedback);

#endif
