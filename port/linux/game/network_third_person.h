#ifndef NETWORK_THIRD_PERSON_H
#define NETWORK_THIRD_PERSON_H
#include "cseries.h"
/* whether a multiplayer game leaves the view over the shoulder to each
player (network.third_person): the host's, which its clients are told */
void network_third_person_reset(void);
void network_third_person_update(void);
void network_third_person_send(long machine_index);
boolean network_third_person_allowed(void);
boolean network_third_person_handles_message(word const *message, word size);
void network_third_person_handle_message(long machine_index, word const *message, word size);
#endif
