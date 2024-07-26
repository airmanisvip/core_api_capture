#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
	unsigned long long os_gettime_ns();
	bool os_sleep_fastto_ns(unsigned long long target_ns);
#ifdef __cplusplus
}
#endif