#ifndef BIMODE_H
#define BIMODE_H

#include <stdint.h>
#include <stdlib.h>
#include "predictor.h"

uint64_t ghistory;

int bimode_nt_ghistoryBits = 11;
int bimode_t_ghistoryBits = 11;
int ct_PCBits = 11;

uint8_t *pht_nt_bimode;
uint8_t *pht_t_bimode;
uint8_t *ct_bimode;

void init_bimode();

uint8_t bimode_predict(uint32_t pc);

void train_bimode(uint32_t pc, uint8_t outcome);

void cleanup_bimode();

#endif