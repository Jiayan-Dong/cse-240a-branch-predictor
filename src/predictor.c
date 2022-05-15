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

uint8_t
gshare_predict(uint32_t pc)
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

uint8_t
alpha21264_predict(uint32_t pc)
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
    init_alpha21264();
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
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
    return alpha21264_predict(pc);
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
    return train_alpha21264(pc, outcome);
  default:
    break;
  }
}
