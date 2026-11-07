/*******************************************************************************
 * Copyright (c) 2017, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "random.h"
#include "opener_api.h"
#include <stdlib.h>

Random *RandomNew(SetSeed set_seed,
                  GetNextUInt32 get_next_uint32) {
  Random *random = CipCalloc(1,sizeof(Random) );
  *random =
    (Random ) { .set_seed = set_seed, .get_next_uint32 = get_next_uint32 };
  return random;
}

void RandomDelete(Random **random) {
  CipFree(*random);
  *random = NULL;
}
