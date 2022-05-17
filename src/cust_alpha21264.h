#ifndef CUST_ALPHA21264_H
#define CUST_ALPHA21264_H

#include <stdint.h>
#include <stdlib.h>
#include "predictor.h"

// cust_alpha21264
uint64_t *lht_cust_alpha21264;
uint8_t *lpt_cust_alpha21264;

uint8_t *gpt_cust_alpha21264;

uint8_t *ct_cust_alpha21264;

int cust_alpha21264LhistoryBits = 10; // Number of bits of saturating counter used for Local prediction
int cust_alpha21264LIndexBits = 10;   // Number of Program counter bits used for Local history table
int cust_alpha21264ChoiceBits = 12;   // Number of Path history bits used for Choice prediction

void init_cust_alpha21264();

uint8_t cust_alpha21264_predict(uint32_t pc);

void train_cust_alpha21264(uint32_t pc, uint8_t outcome);

void cleanup_cust_alpha21264();

#endif