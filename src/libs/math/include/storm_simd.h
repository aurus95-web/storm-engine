#pragma once

// SSE intrinsics are used by the math code. On ARM (Apple Silicon, iOS) they are
// translated to NEON by sse2neon (https://github.com/DLTcollab/sse2neon, v1.8.0, MIT).
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__)
#include "third_party/sse2neon.h"
#else
#include <xmmintrin.h>
#endif
