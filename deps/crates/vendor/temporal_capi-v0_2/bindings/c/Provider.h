#ifndef Provider_H
#define Provider_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "Provider.d.h"






typedef struct temporal_rs_Provider_new_zoneinfo64_result {union {Provider* ok; }; bool is_ok;} temporal_rs_Provider_new_zoneinfo64_result;
temporal_rs_Provider_new_zoneinfo64_result temporal_rs_Provider_new_zoneinfo64(DiplomatU32View data);

Provider* temporal_rs_Provider_new_compiled(void);

Provider* temporal_rs_Provider_empty(void);

void temporal_rs_Provider_destroy(Provider* self);





#endif // Provider_H
