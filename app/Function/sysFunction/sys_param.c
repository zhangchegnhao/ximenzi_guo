#include "sys_param.h"
#include "sys_storage.h"

static float s_ratio_ch0  = SYS_PARAM_RATIO_DEFAULT;
static float s_ratio_ch1  = SYS_PARAM_RATIO_DEFAULT;
static float s_thresh_ch0 = SYS_PARAM_THRESH_DEFAULT;
static float s_thresh_ch1 = SYS_PARAM_THRESH_DEFAULT;

float sys_param_get_ch0_ratio(void)        { return s_ratio_ch0; }
void  sys_param_set_ch0_ratio(float ratio) { s_ratio_ch0 = ratio; sys_storage_save(); }

float sys_param_get_ch1_ratio(void)        { return s_ratio_ch1; }
void  sys_param_set_ch1_ratio(float ratio) { s_ratio_ch1 = ratio; sys_storage_save(); }

float sys_param_get_ch0_thresh(void)       { return s_thresh_ch0; }
void  sys_param_set_ch0_thresh(float th)   { s_thresh_ch0 = th; sys_storage_save(); }

float sys_param_get_ch1_thresh(void)       { return s_thresh_ch1; }
void  sys_param_set_ch1_thresh(float th)   { s_thresh_ch1 = th; sys_storage_save(); }
