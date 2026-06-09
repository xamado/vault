#ifndef FALLOUT_PLIB_MATH_H_
#define FALLOUT_PLIB_MATH_H_

// Small math helpers. Lowercase `min`/`max` match the convention used by the
// original Win32 source (those macros came from <windef.h>), so existing
// decompiled call sites work without renaming.
//
// NOTE: macros, not inline functions, so the conditional propagates correctly
// when called with side-effect-free expressions. Callers should not pass
// expressions with side effects (double-evaluation).

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#endif /* FALLOUT_PLIB_MATH_H_ */
