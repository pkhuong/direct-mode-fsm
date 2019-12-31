#include "imsm_ppoint.h"

#include "imsm.h"

extern size_t imsm_index(struct imsm_ctx *ctx,
    struct imsm_ppoint_record record);

extern struct imsm_unwind_record imsm_region_push(struct imsm_ctx *,
    struct imsm_ppoint_record);

extern void imsm_region_pop(const struct imsm_unwind_record *);
