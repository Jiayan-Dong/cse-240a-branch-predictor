#include "predictor.h"
#include <string.h>
#include <stdio.h>

uint64_t ghistory;

int bimode_nt_ghistoryBits = 11;
int bimode_t_ghistoryBits = 11;
int ct_PCBits = 11;

uint8_t *pht_nt_bimode;
uint8_t *pht_t_bimode;
uint8_t *ct_bimode;

// bimode functions
void init_bimode()
{
  int pht_nt_entries = 1 << bimode_nt_ghistoryBits;                    // 2^10 entries in LHT (Local histort table)
  int pht_t_entries = 1 << bimode_t_ghistoryBits;                      // 2^11 entries in LPT (Local prediction table)
  int choice_entries = 1 << ct_PCBits;                                 // 2^12 entries in CT (Choice prediction table)
  pht_nt_bimode = (uint8_t *)malloc(pht_nt_entries * sizeof(uint8_t)); // 2^11 * 2 (bit counter) = 4 * 2^10 bits
  pht_t_bimode = (uint8_t *)malloc(pht_t_entries * sizeof(uint8_t));   // 2^12 * 2 (bit counter) = 8 * 2^10 bits
  ct_bimode = (uint8_t *)malloc(choice_entries * sizeof(uint8_t));     // 2^12 * 2 (bit counter) = 8 * 2^10 bits
                                                                       // total: (11 + 4 + 8 + 8) * 2^10 bits = 31 * 2^10 bits = 31744 bits < 32Kbits = 32768 bits

  memset(pht_nt_bimode, WN, pht_nt_entries);
  memset(pht_t_bimode, WN, pht_t_entries);
  memset(ct_bimode, WN, choice_entries);

  ghistory = 0;
}

uint8_t bimode_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t pht_nt_entries = 1 << bimode_nt_ghistoryBits;
  uint32_t pc_lower_bits_nt = pc & (pht_nt_entries - 1);
  uint32_t ghistory_lower_bits_nt = ghistory & (pht_nt_entries - 1);
  uint32_t index_nt = pc_lower_bits_nt ^ ghistory_lower_bits_nt;

  uint32_t pht_t_entries = 1 << bimode_t_ghistoryBits;
  uint32_t pc_lower_bits_t = pc & (pht_t_entries - 1);
  uint32_t ghistory_lower_bits_t = ghistory & (pht_t_entries - 1);
  uint32_t index_t = pc_lower_bits_t ^ ghistory_lower_bits_t;

  uint32_t ct_entries = 1 << ct_PCBits;
  uint32_t ct_index = ghistory & (ct_entries - 1);

  if (ct_bimode[ct_index] >= SN && ct_bimode[ct_index] <= WN)
  {
    switch (pht_nt_bimode[index_nt])
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
      printf("Warning: Undefined state of entry in Bimode PHT NT!\n");
      return NOTTAKEN;
    }
  }
  else if (ct_bimode[ct_index] >= WT && ct_bimode[ct_index] <= ST)
  {
    switch (pht_t_bimode[index_t])
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
      printf("Warning: Undefined state of entry in Bimode PHT T!\n");
      return NOTTAKEN;
    }
  }
  else
  {
    printf("Warning: Undefined state of entry in Bimode CT!\n");
    return NOTTAKEN;
  }
}

void train_bimode(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t pht_nt_entries = 1 << bimode_nt_ghistoryBits;
  uint32_t pc_lower_bits_nt = pc & (pht_nt_entries - 1);
  uint32_t ghistory_lower_bits_nt = ghistory & (pht_nt_entries - 1);
  uint32_t index_nt = pc_lower_bits_nt ^ ghistory_lower_bits_nt;

  uint32_t pht_t_entries = 1 << bimode_t_ghistoryBits;
  uint32_t pc_lower_bits_t = pc & (pht_t_entries - 1);
  uint32_t ghistory_lower_bits_t = ghistory & (pht_t_entries - 1);
  uint32_t index_t = pc_lower_bits_t ^ ghistory_lower_bits_t;

  uint32_t ct_entries = 1 << ct_PCBits;
  uint32_t ct_index = ghistory & (ct_entries - 1);

  uint8_t wrong = 0;

  if (ct_bimode[ct_index] >= SN && ct_bimode[ct_index] <= WN)
  {
    // Update state of entry in bht based on outcome
    switch (pht_nt_bimode[index_nt])
    {
    case WN:
      pht_nt_bimode[index_nt] = (outcome == TAKEN) ? WT : SN;
      wrong = 1;
      break;
    case SN:
      pht_nt_bimode[index_nt] = (outcome == TAKEN) ? WN : SN;
      wrong = 1;
      break;
    case WT:
      pht_nt_bimode[index_nt] = (outcome == TAKEN) ? ST : WN;
      break;
    case ST:
      pht_nt_bimode[index_nt] = (outcome == TAKEN) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    }
  }
  else if (ct_bimode[ct_index] >= WT && ct_bimode[ct_index] <= ST)
  {
    // Update state of entry in bht based on outcome
    switch (pht_t_bimode[index_t])
    {
    case WN:
      pht_t_bimode[index_t] = (outcome == TAKEN) ? WT : SN;
      wrong = 1;
      break;
    case SN:
      pht_t_bimode[index_t] = (outcome == TAKEN) ? WN : SN;
      wrong = 1;
      break;
    case WT:
      pht_t_bimode[index_t] = (outcome == TAKEN) ? ST : WN;
      break;
    case ST:
      pht_t_bimode[index_t] = (outcome == TAKEN) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    }
  }
  else
  {
    printf("Warning: Undefined state of entry in Bimode CT!\n");
  }
  if (wrong == 1 | ct_bimode[ct_index] == outcome)
  {
    switch (ct_bimode[ct_index])
    {
    case WN:
      ct_bimode[ct_index] = (outcome == TAKEN) ? WT : SN;
      break;
    case SN:
      ct_bimode[ct_index] = (outcome == TAKEN) ? WN : SN;
      break;
    case WT:
      ct_bimode[ct_index] = (outcome == TAKEN) ? ST : WN;
      break;
    case ST:
      ct_bimode[ct_index] = (outcome == TAKEN) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry in Bimode CT!\n");
    }
  }
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_bimode()
{
  free(pht_nt_bimode);
  free(pht_t_bimode);
  free(ct_bimode);
}