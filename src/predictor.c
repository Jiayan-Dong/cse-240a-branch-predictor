//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Jiayan Dong";
const char *studentID = "A16593051";
const char *email = "jid001@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 14; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

int alpha21264GhistoryBits = 12; // Number of Path history bits used for Global prediction
int alpha21264LhistoryBits = 11; // Number of bits of saturating counter used for Local prediction
int alpha21264LIndexBits = 10;   // Number of Program counter bits used for Local history table
int alpha21264ChoiceBits = 12;   // Number of Path history bits used for Choice prediction

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//
// gshare
uint8_t *bht_gshare;
uint64_t ghistory;

// Alpha21264
uint64_t *lht_alpha21264;
uint8_t *lpt_alpha21264;

uint8_t *gpt_alpha21264;

uint8_t *ct_alpha21264;

// Custom: TAGE
#define TAGE_GHR_SIZE_QW 4
#define COMPONENT_NUM 7

int T0_PC = 10;
int Ti_PC = 8;
int tag_bit = 11;
int u_bit = 2;
int pred_bit = 3;

uint64_t tage_ghr[TAGE_GHR_SIZE_QW];

uint64_t get_bit_in_tage_ghr(uint64_t idx)
{
  int q = idx / 64;
  int r = idx % 64;
  return (tage_ghr[q] << (63 - r)) >> 63;
}

void update_tage_ghr(uint64_t outcome)
{
  uint64_t msb[TAGE_GHR_SIZE_QW];
  for (size_t i = 0; i < TAGE_GHR_SIZE_QW; i++)
  {
    msb[i] = tage_ghr[i] >> 63;
    tage_ghr[i] = tage_ghr[i] << 1;
  }
  tage_ghr[0] = tage_ghr[0] | outcome;
  for (size_t i = 1; i < TAGE_GHR_SIZE_QW; i++)
  {
    tage_ghr[i] = tage_ghr[i] | msb[i - 1];
  }
}

struct circular_shift_register
{
  uint32_t r;
  uint8_t bits;
};

void update_csr(struct circular_shift_register *p, uint64_t used_length)
{
  uint64_t mask = 1;
  mask = (mask << p->bits) - 1;
  uint64_t new_lsb = (p->r >> (p->bits - 1)) ^ (tage_ghr[0] & 1);
  p->r = (p->r << 1) & mask | new_lsb;
  p->r = p->r ^ (get_bit_in_tage_ghr(used_length) << (used_length % p->bits));
}

struct saturating_counter
{
  uint8_t ctr;
  uint8_t bits;
};

uint8_t get_saturating_counter(struct saturating_counter *p)
{
  if (p->ctr & (1 << (p->bits - 1)))
  {
    return TAKEN;
  }
  else
  {
    return NOTTAKEN;
  }
}

void inc_saturating_counter(struct saturating_counter *p)
{
  if (p->ctr == ((1 << p->bits) - 1))
  {
    return;
  }
  else
  {
    p->ctr += 1;
  }
}

void dec_saturating_counter(struct saturating_counter *p)
{
  if (p->ctr == 0)
  {
    return;
  }
  else
  {
    p->ctr -= 1;
  }
}

struct tage_component_entry
{
  struct saturating_counter pred;
  struct saturating_counter u;
  uint64_t tag;
};

struct saturating_counter *T0;
struct tage_component_entry *tage_component[COMPONENT_NUM];
uint64_t tage_component_used_length[COMPONENT_NUM] = {5, 9, 15, 25, 44, 76, 130};
struct circular_shift_register csr0[COMPONENT_NUM];
struct circular_shift_register csr1[COMPONENT_NUM];
struct circular_shift_register csr2[COMPONENT_NUM];

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

// gshare functions
void init_gshare()
{
  int bht_entries = 1 << ghistoryBits;                           // 2^14 entries in BHT
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t)); // 2^14 * 2 (bit counter) = 2^15 = 32 Kb
  int i = 0;
  for (i = 0; i < bht_entries; i++)
  {
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch (bht_gshare[index])
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
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  // Update state of entry in bht based on outcome
  switch (bht_gshare[index])
  {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}

// alpha21264 functions
void init_alpha21264()
{
  int lht_entries = 1 << alpha21264LIndexBits;                         // 2^10 entries in LHT (Local histort table)
  int lpt_entries = 1 << alpha21264LhistoryBits;                       // 2^11 entries in LPT (Local prediction table)
  int gpt_entries = 1 << alpha21264GhistoryBits;                       // 2^12 entries in GPT (Global prediction table)
  int choice_entries = 1 << alpha21264ChoiceBits;                      // 2^12 entries in CT (Choice prediction table)
  lht_alpha21264 = (uint64_t *)calloc(lht_entries, sizeof(uint64_t));  // 2^10 * 11 (bit counter) = 11 * 2^10 bits
  lpt_alpha21264 = (uint8_t *)malloc(lpt_entries * sizeof(uint8_t));   // 2^11 * 2 (bit counter) = 4 * 2^10 bits
  gpt_alpha21264 = (uint8_t *)malloc(gpt_entries * sizeof(uint8_t));   // 2^12 * 2 (bit counter) = 8 * 2^10 bits
  ct_alpha21264 = (uint8_t *)malloc(choice_entries * sizeof(uint8_t)); // 2^12 * 2 (bit counter) = 8 * 2^10 bits
                                                                       // total: (11 + 4 + 8 + 8) * 2^10 bits = 31 * 2^10 bits = 31744 bits < 32Kbits = 32768 bits

  memset(lpt_alpha21264, WN, lht_entries);
  memset(gpt_alpha21264, WN, gpt_entries);
  memset(ct_alpha21264, WN, choice_entries);

  ghistory = 0;
}

uint8_t alpha21264_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t lht_entries = 1 << alpha21264LIndexBits;
  uint32_t lht_index = pc & (lht_entries - 1);

  uint32_t lpt_entries = 1 << alpha21264LhistoryBits;
  uint32_t lpt_index = lht_alpha21264[lht_index] & (lpt_entries - 1);

  uint32_t gpt_entries = 1 << alpha21264GhistoryBits;
  uint32_t gpt_index = ghistory & (gpt_entries - 1);

  uint32_t ct_entries = 1 << alpha21264ChoiceBits;
  uint32_t ct_index = ghistory & (ct_entries - 1);

  if (ct_alpha21264[ct_index] >= SN && ct_alpha21264[ct_index] <= WN)
  {
    switch (lpt_alpha21264[lpt_index])
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
      printf("Warning: Undefined state of entry in Alpha21264 LPT!\n");
      return NOTTAKEN;
    }
  }
  else if (ct_alpha21264[ct_index] >= WT && ct_alpha21264[ct_index] <= ST)
  {
    switch (gpt_alpha21264[gpt_index])
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
      printf("Warning: Undefined state of entry in Alpha21264 GPT!\n");
      return NOTTAKEN;
    }
  }
  else
  {
    printf("Warning: Undefined state of entry in Alpha21264 CT!\n");
    return NOTTAKEN;
  }
}

void train_alpha21264(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t lht_entries = 1 << alpha21264LIndexBits;
  uint32_t lht_index = pc & (lht_entries - 1);

  uint32_t lpt_entries = 1 << alpha21264LhistoryBits;
  uint32_t lpt_index = lht_alpha21264[lht_index] & (lpt_entries - 1);

  uint32_t gpt_entries = 1 << alpha21264GhistoryBits;
  uint32_t gpt_index = ghistory & (gpt_entries - 1);

  uint32_t ct_entries = 1 << alpha21264ChoiceBits;
  uint32_t ct_index = ghistory & (ct_entries - 1);

  uint8_t lpt_outcome;
  uint8_t gpt_outcome;

  // Update state of entry in bht based on outcome
  switch (lpt_alpha21264[lpt_index])
  {
  case WN:
    lpt_alpha21264[lpt_index] = (outcome == TAKEN) ? WT : SN;
    lpt_outcome = NOTTAKEN;
    break;
  case SN:
    lpt_alpha21264[lpt_index] = (outcome == TAKEN) ? WN : SN;
    lpt_outcome = NOTTAKEN;
    break;
  case WT:
    lpt_alpha21264[lpt_index] = (outcome == TAKEN) ? ST : WN;
    lpt_outcome = TAKEN;
    break;
  case ST:
    lpt_alpha21264[lpt_index] = (outcome == TAKEN) ? ST : WT;
    lpt_outcome = TAKEN;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  switch (gpt_alpha21264[gpt_index])
  {
  case WN:
    gpt_alpha21264[gpt_index] = (outcome == TAKEN) ? WT : SN;
    gpt_outcome = NOTTAKEN;
    break;
  case SN:
    gpt_alpha21264[gpt_index] = (outcome == TAKEN) ? WN : SN;
    gpt_outcome = NOTTAKEN;
    break;
  case WT:
    gpt_alpha21264[gpt_index] = (outcome == TAKEN) ? ST : WN;
    gpt_outcome = TAKEN;
    break;
  case ST:
    gpt_alpha21264[gpt_index] = (outcome == TAKEN) ? ST : WT;
    gpt_outcome = TAKEN;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  if ((lpt_alpha21264[lpt_index] == ST || lpt_alpha21264[lpt_index] == WT) ^ (gpt_alpha21264[gpt_index] == ST || gpt_alpha21264[gpt_index] == WT))
  {
    switch (ct_alpha21264[ct_index])
    {
    case WN:
      ct_alpha21264[ct_index] = (outcome == lpt_outcome) ? SN : WT;
      break;
    case SN:
      ct_alpha21264[ct_index] = (outcome == lpt_outcome) ? SN : WN;
      break;
    case WT:
      ct_alpha21264[ct_index] = (outcome == gpt_outcome) ? ST : WN;
      break;
    case ST:
      ct_alpha21264[ct_index] = (outcome == gpt_outcome) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    }
  }

  // Update history register
  lht_alpha21264[lht_index] = ((lht_alpha21264[lht_index] << 1) | outcome);
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_alpha21264()
{
  free(lht_alpha21264);
  free(lpt_alpha21264);
  free(gpt_alpha21264);
  free(ct_alpha21264);
}

// Custom-tage function
void init_tage()
{
  for (size_t i = 0; i < TAGE_GHR_SIZE_QW; i++)
  {
    tage_ghr[i] = 0;
  }

  uint32_t T0_entries = 1 << T0_PC;
  T0 = (struct saturating_counter *)malloc(T0_entries * sizeof(struct saturating_counter));
  for (size_t i = 0; i < T0_entries; i++)
  {
    T0[i].bits = 2;
    T0[i].ctr = WT;
  }

  uint32_t Ti_entries = 1 << Ti_PC;
  for (size_t i = 0; i < COMPONENT_NUM; i++)
  {
    tage_component[i] = (struct tage_component_entry *)calloc(Ti_entries, sizeof(struct tage_component_entry));
    for (size_t j = 0; j < Ti_entries; j++)
    {
      tage_component[i][j].pred.bits = 3;
      tage_component[i][j].pred.ctr = 4;

      tage_component[i][j].u.bits = 2;
      tage_component[i][j].u.ctr = 0;

      tage_component[i][j].tag = 0;
    }
    csr0[i].bits = Ti_PC;
    csr0[i].r = 0;
    csr1[i].bits = tag_bit;
    csr1[i].r = 0;
    csr2[i].bits = tag_bit;
    csr2[i].r = 0;
  }
}

uint8_t tage_predict(uint32_t pc)
{
  uint32_t Ti_mask = (1 << Ti_PC) - 1;
  uint32_t Ti_index;
  uint64_t tag;
  uint32_t tag_mask = (1 << tag_bit) - 1;
  for (int i = COMPONENT_NUM - 1; i >= 0; i--)
  {
    Ti_index = (pc >> tage_component_used_length[i]) ^ pc ^ csr0[i].r;
    Ti_index = Ti_index & Ti_mask;
    tag = pc ^ csr1[i].r ^ (csr2[i].r << 1);
    tag = tag & tag_mask;
    if (tage_component[i][Ti_index].tag == tag)
    {
      return get_saturating_counter(&tage_component[i][Ti_index].pred);
    }
  }
  uint32_t T0_entries = 1 << T0_PC;
  uint32_t T0_index = pc & (T0_entries - 1);
  return get_saturating_counter(&T0[T0_index]);
}

void train_tage(uint32_t pc, uint8_t outcome)
{
  uint8_t pred_result;

  uint8_t propred = 0;
  uint32_t Ti_indexes[COMPONENT_NUM];
  uint32_t Ti_tags[COMPONENT_NUM];
  uint8_t altpred = 0;

  uint8_t not_found = 1;
  uint32_t Ti_mask = (1 << Ti_PC) - 1;
  uint32_t Ti_index;
  uint32_t tag;
  uint32_t tag_mask = (1 << tag_bit) - 1;
  for (int i = COMPONENT_NUM - 1; i >= 0; i--)
  {
    Ti_index = (pc >> tage_component_used_length[i]) ^ pc ^ csr0[i].r;
    Ti_index = Ti_index & Ti_mask;
    Ti_indexes[i] = Ti_index;
    tag = pc ^ csr1[i].r ^ (csr2[i].r << 1);
    tag = tag & tag_mask;
    Ti_tags[i] = tag;
    if (tage_component[i][Ti_index].tag == tag)
    {
      if (propred == 0)
      {
        propred = i;
        not_found = 0;
        pred_result = get_saturating_counter(&tage_component[i][Ti_index].pred);
      }
      else if (altpred == 0)
      {
        altpred = i;
        break;
      }
    }
  }
  if (not_found)
  {
    uint32_t T0_entries = 1 << T0_PC;
    uint32_t T0_index = pc & (T0_entries - 1);
    pred_result = get_saturating_counter(&T0[T0_index]);
  }

  if (propred != 0 && propred != altpred)
  {
    if (pred_result == outcome)
    {
      inc_saturating_counter(&tage_component[propred][Ti_indexes[propred]].u);
    }
    else
    {
      dec_saturating_counter(&tage_component[propred][Ti_indexes[propred]].u);
    }
  }

  if (pred_result != outcome)
  {
    dec_saturating_counter(&tage_component[propred][Ti_indexes[propred]].pred);
    if (propred != COMPONENT_NUM - 1)
    {
      int comp_num = 0;
      not_found = 1;
      for (size_t i = propred + 1; i < COMPONENT_NUM; i++)
      {
        if (tage_component[i][Ti_indexes[i]].u.ctr == 0)
        {
          not_found = 0;
          comp_num++;
        }
      }
      if (comp_num > 0)
      {
        comp_num = (1 << comp_num) - 1;
        int r = rand() % comp_num;
        for (size_t i = propred + 1; i < COMPONENT_NUM; i++)
        {
          if (tage_component[i][Ti_indexes[i]].u.ctr == 0)
          {
            if ((r & 1) == 0)
            {
              tage_component[i][Ti_indexes[i]].pred.ctr = 4; // hard code
              tage_component[i][Ti_indexes[i]].tag = Ti_tags[i];
              break;
            }
            else
            {
              r = r >> 1;
            }
          }
        }
      }
    }
    if (not_found)
    {
      for (size_t i = propred + 1; i < COMPONENT_NUM; i++)
      {
        dec_saturating_counter(&tage_component[i][Ti_indexes[i]].u);
      }
    }
  }

  // Update history register and csr
  update_tage_ghr(outcome);
  for (size_t i = 0; i < COMPONENT_NUM; i++)
  {
    update_csr(&csr0[i], tage_component_used_length[i]);
    update_csr(&csr1[i], tage_component_used_length[i]);
    update_csr(&csr2[i], tage_component_used_length[i]);
  }
}

void cleanup_tage()
{
  free(T0);

  for (size_t i = 0; i < COMPONENT_NUM; i++)
  {
    free(tage_component[i]);
  }
}

void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_alpha21264();
    break;
  case CUSTOM:
    init_tage();
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return alpha21264_predict(pc);
  case CUSTOM:
    return tage_predict(pc);
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint8_t outcome)
{

  switch (bpType)
  {
  case STATIC:
  case GSHARE:
    return train_gshare(pc, outcome);
  case TOURNAMENT:
    return train_alpha21264(pc, outcome);
  case CUSTOM:
    return train_tage(pc, outcome);
  default:
    break;
  }
}
