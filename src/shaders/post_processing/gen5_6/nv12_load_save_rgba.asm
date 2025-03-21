// Module name: NV12_LOAD_SAVE_RGBA
.kernel NV12_LOAD_SAVE_RGBA
.code

#define FIX_POINT_CONVERSION
#define FORCE_ALPHA_TO_ONE

#include "SetupVPKernel.asm"
#include "YUV_to_RGBX_Coef.asm"
#include "Multiple_Loop_Head.asm"
#include "NV12_Load_8x4.asm"
#include "YUVX_Save_RGBX_Fix.asm"
#include "RGB16x8_Save_RGB.asm"
#include "Multiple_Loop.asm"

END_THREAD  // End of Thread

.end_code  

.end_kernel

// end of nv12_load_save_rgba.asm
