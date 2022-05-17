#include "bimode.h"
#include "predictor.h"
#include <string.h>
#include <stdio.h>

// cust_alpha21264
uint64_t *lht_cust_alpha21264;
uint8_t *lpt_cust_alpha21264;

uint8_t *gpt_cust_alpha21264;

uint8_t *ct_cust_alpha21264;

int cust_alpha21264LhistoryBits = 10; // Number of bits of saturating counter used for Local prediction
int cust_alpha21264LIndexBits = 10;   // Number of Program counter bits used for Local history table
int cust_alpha21264ChoiceBits = 12;   // Number of Path history bits used for Choice prediction

// cust_alpha21264 functions
void init_cust_alpha21264()
{
  int lht_entries = 1 << cust_alpha21264LIndexBits;                         // 2^10 entries in LHT (Local histort table)
  int lpt_entries = 1 << cust_alpha21264LhistoryBits;                       // 2^10 entries in LPT (Local prediction table)
  int choice_entries = 1 << cust_alpha21264ChoiceBits;                      // 2^12 entries in CT (Choice prediction table)
  lht_cust_alpha21264 = (uint64_t *)calloc(lht_entries, sizeof(uint64_t));  // 2^10 * 10 (bit counter) = 10 * 2^10 bits
  lpt_cust_alpha21264 = (uint8_t *)malloc(lpt_entries * sizeof(uint8_t));   // 2^10 * 2 (bit counter) = 2 * 2^10 bits
  ct_cust_alpha21264 = (uint8_t *)malloc(choice_entries * sizeof(uint8_t)); // 2^12 * 2 (bit counter) = 8 * 2^10 bits

  memset(lpt_cust_alpha21264, WN, lht_entries);
  memset(ct_cust_alpha21264, WN, choice_entries);

  bimode_nt_ghistoryBits = 11;
  bimode_t_ghistoryBits = 11;
  ct_PCBits = 11;

  init_bimode(); // 3 tables, 3 * 2^11 * 2 = 12 * 2^10
                 // total: (10 + 2 + 8 + 12) * 2^10 = 2^15 = 32Kbits, + max global history register = 11 bits
                 // budgets: 32Kbits + 11 bits
  ghistory = 0;
}

uint8_t cust_alpha21264_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t lht_entries = 1 << cust_alpha21264LIndexBits;
  uint32_t lht_index = pc & (lht_entries - 1);

  uint32_t lpt_entries = 1 << cust_alpha21264LhistoryBits;
  uint32_t lpt_index = lht_cust_alpha21264[lht_index] & (lpt_entries - 1);

  uint32_t ct_entries = 1 << cust_alpha21264ChoiceBits;
  uint32_t ct_index = ghistory & (ct_entries - 1);

  if (ct_cust_alpha21264[ct_index] >= SN && ct_cust_alpha21264[ct_index] <= WN)
  {
    switch (lpt_cust_alpha21264[lpt_index])
    {
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in cust_alpha21264 LPT!\n");
      return NOTTAKEN;
    }
  }
  else if (ct_cust_alpha21264[ct_index] >= WT && ct_cust_alpha21264[ct_index] <= ST)
  {
    return bimode_predict(pc);
  }
  else
  {
    printf("Warning: Undefined state of entry in cust_alpha21264 CT!\n");
    return NOTTAKEN;
  }
}

void train_cust_alpha21264(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t lht_entries = 1 << cust_alpha21264LIndexBits;
  uint32_t lht_index = pc & (lht_entries - 1);

  uint32_t lpt_entries = 1 << cust_alpha21264LhistoryBits;
  uint32_t lpt_index = lht_cust_alpha21264[lht_index] & (lpt_entries - 1);

  uint32_t ct_entries = 1 << cust_alpha21264ChoiceBits;
  uint32_t ct_index = ghistory & (ct_entries - 1);

  uint8_t lpt_outcome;
  uint8_t bimode_outcome = bimode_predict(pc);

  // Update state of entry in bht based on outcome
  switch (lpt_cust_alpha21264[lpt_index])
  {
  case WN:
    lpt_cust_alpha21264[lpt_index] = (outcome == TAKEN) ? WT : SN;
    lpt_outcome = NOTTAKEN;
    break;
  case SN:
    lpt_cust_alpha21264[lpt_index] = (outcome == TAKEN) ? WN : SN;
    lpt_outcome = NOTTAKEN;
    break;
  case WT:
    lpt_cust_alpha21264[lpt_index] = (outcome == TAKEN) ? ST : WN;
    lpt_outcome = TAKEN;
    break;
  case ST:
    lpt_cust_alpha21264[lpt_index] = (outcome == TAKEN) ? ST : WT;
    lpt_outcome = TAKEN;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  train_bimode(pc, outcome);
  ghistory = (ghistory >> 1);

  if ((lpt_cust_alpha21264[lpt_index] == ST || lpt_cust_alpha21264[lpt_index] == WT) ^ (bimode_outcome == TAKEN))
  {
    switch (ct_cust_alpha21264[ct_index])
    {
    case WN:
      ct_cust_alpha21264[ct_index] = (outcome == lpt_outcome) ? SN : WT;
      break;
    case SN:
      ct_cust_alpha21264[ct_index] = (outcome == lpt_outcome) ? SN : WN;
      break;
    case WT:
      ct_cust_alpha21264[ct_index] = (outcome == bimode_outcome) ? ST : WN;
      break;
    case ST:
      ct_cust_alpha21264[ct_index] = (outcome == bimode_outcome) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    }
  }

  // Update history register
  lht_cust_alpha21264[lht_index] = ((lht_cust_alpha21264[lht_index] << 1) | outcome);
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_cust_alpha21264()
{
  free(lht_cust_alpha21264);
  free(lpt_cust_alpha21264);
  free(gpt_cust_alpha21264);
  free(ct_cust_alpha21264);
  cleanup_bimode();
}