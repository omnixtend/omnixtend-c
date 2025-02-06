#ifndef __FLOWCONTROL_H__
#define __FLOWCONTROL_H__

#define NUM_CHANNELS	5
#define DEFAULT_CREDIT  10

typedef struct {
	unsigned int available_credits[NUM_CHANNELS];
} FlowControlCredit;

FlowControlCredit *create_credit();
void set_credit(FlowControlCredit *fc, int channel, int credit);
int check_all_channels(FlowControlCredit *fc);
int dec_credit(FlowControlCredit *fc, int channel, int credit);
void inc_credit(FlowControlCredit *fc, int channel, int credit);
int get_credit(FlowControlCredit *fc, const int channel);

#endif // __FLOWCONTROL_H__
