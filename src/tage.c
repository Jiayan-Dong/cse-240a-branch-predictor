#include "predictor.h"

// Custom: TAGE
#define TAGE_GHR_SIZE_QW 16
#define COMPONENT_NUM 7

int T0_PC = 11; // 2^11 * 2 = 4 * 2^10
int Ti_PC = 8;  // Ti(1-7): 7 * (11+2+3) * 2^8 = 28 * 2^10
int tag_bit = 11;
int u_bit = 2;
int pred_bit = 3; // Total: 4 * 2^10 + 28 * 2^10 = 2^15 = 32Kbits
                  // With max history 130
                  // if using csr: len 5 doesn't need one, len 9 only need 1 8-bit csr
                  // the other len need (8+11+10) bits, total 103 bits
                  // with one possible linear feedback shift register 16 bits
                  // total = 130 + 103 + 16 = 249 bits <= 320 bits
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

void update_saturating_counter(struct saturating_counter *p, uint8_t outcome)
{
  if (outcome == TAKEN)
  {
    inc_saturating_counter(p);
  }
  else
  {
    dec_saturating_counter(p);
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
// uint64_t tage_component_used_length[COMPONENT_NUM] = {5, 9, 15, 25, 44, 76, 130};
uint64_t tage_component_used_length[COMPONENT_NUM] = {4, 9, 15, 25, 44, 76, 150};
struct circular_shift_register csr0[COMPONENT_NUM];
struct circular_shift_register csr1[COMPONENT_NUM];
struct circular_shift_register csr2[COMPONENT_NUM];

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
    T0[i].ctr = WN;
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
  uint8_t pred;
  uint8_t altpred;

  int8_t pred_index = -1;
  int8_t altpred_index = -1;
  uint32_t Ti_indexes[COMPONENT_NUM];
  uint32_t Ti_tags[COMPONENT_NUM];

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
      if (pred_index == -1)
      {
        not_found = 0;
        pred_index = i;
        pred = get_saturating_counter(&tage_component[i][Ti_index].pred);
      }
      else if (altpred_index == -1)
      {
        altpred_index = i;
        altpred = get_saturating_counter(&tage_component[i][Ti_index].pred);
        break;
      }
    }
  }
  uint32_t T0_entries = 1 << T0_PC;
  uint32_t T0_index = pc & (T0_entries - 1);
  if (pred_index == -1)
  {
    pred = get_saturating_counter(&T0[T0_index]);
  }

  if (altpred_index == -1)
  {
    altpred = get_saturating_counter(&T0[T0_index]);
  }

  if (pred_index != -1 && pred != altpred)
  {
    if (pred == outcome)
    {
      inc_saturating_counter(&tage_component[pred_index][Ti_indexes[pred_index]].u);
    }
    else
    {
      dec_saturating_counter(&tage_component[pred_index][Ti_indexes[pred_index]].u);
    }
  }

  if (pred == outcome)
  {
    if (pred_index != -1)
    {
      if (outcome == TAKEN)
      {
        inc_saturating_counter(&tage_component[pred_index][Ti_indexes[pred_index]].pred);
      }
      else if (outcome == NOTTAKEN)
      {
        dec_saturating_counter(&tage_component[pred_index][Ti_indexes[pred_index]].pred);
      }
    }
    else
    {
      if (outcome == TAKEN)
      {
        inc_saturating_counter(&T0[T0_index]);
      }
      else if (outcome == NOTTAKEN)
      {
        dec_saturating_counter(&T0[T0_index]);
      }
    }
  }
  else if (pred != outcome)
  {
    if (pred_index != -1)
    {
      if (outcome == TAKEN)
      {
        inc_saturating_counter(&tage_component[pred_index][Ti_indexes[pred_index]].pred);
      }
      else if (outcome == NOTTAKEN)
      {
        dec_saturating_counter(&tage_component[pred_index][Ti_indexes[pred_index]].pred);
      }
    }
    else
    {
      if (outcome == TAKEN)
      {
        inc_saturating_counter(&T0[T0_index]);
      }
      else if (outcome == NOTTAKEN)
      {
        dec_saturating_counter(&T0[T0_index]);
      }
    }

    if (pred_index != COMPONENT_NUM - 1)
    {
      int comp_num = 0;
      not_found = 1;
      for (size_t i = pred_index + 1; i < COMPONENT_NUM; i++)
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
        for (size_t i = pred_index + 1; i < COMPONENT_NUM; i++)
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
      for (size_t i = pred_index + 1; i < COMPONENT_NUM; i++)
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