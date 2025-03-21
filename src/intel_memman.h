#ifndef _INTEL_MEMMAN_H_
#define _INTEL_MEMMAN_H_

Bool intel_memman_init(struct intel_driver_data *intel);
Bool intel_memman_terminate(struct intel_driver_data *intel);

inline drm_intel_bo *memman_bo_alloc(drm_intel_bufmgr *bufmgr, const char *name,
	unsigned long size, unsigned int alignment);

#endif /* _INTEL_MEMMAN_H_ */
