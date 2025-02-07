#include "flowcontrol.h"
#include "tloe_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FlowControlCredit *create_credit() {
	FlowControlCredit *fc = (FlowControlCredit *) malloc (sizeof(FlowControlCredit));
	memset(fc, 0, sizeof(FlowControlCredit));

	return fc;
}

void set_credit(FlowControlCredit *fc, int channel, int credit) {
	fc->available_credits[channel] = (1 << credit);
}

int check_all_channels(FlowControlCredit *fc) {
	int result = 0;
	printf("Wait until all channels have their credits set.....\n");

	while(!(fc->available_credits[CHANNEL_A] && \
		fc->available_credits[CHANNEL_B] && \
		fc->available_credits[CHANNEL_C] && \
		fc->available_credits[CHANNEL_D] && \
		fc->available_credits[CHANNEL_E])){
	}

	return result;
}

int dec_credit(FlowControlCredit *fc, int channel, int credit) {
	return 1;
	int result = 0;
	unsigned int flit_cnt = (1 << credit);

	if (fc->available_credits[channel] >= flit_cnt) {
		fc->available_credits[channel] -= flit_cnt;
		result = 1;
	}

	return result;
}

void inc_credit(FlowControlCredit *fc, int channel, int credit) {
	unsigned int flit_cnt = (1 << credit);

	fc->available_credits[channel] += flit_cnt;
}

int get_credit(FlowControlCredit *fc, const int channel) {
	return fc->available_credits[channel];
}
