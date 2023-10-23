#pragma once
#include "xparameters.h"
#include "xstatus.h"
#include "xil_io.h"
#include "xil_assert.h"
#include "xuartps_hw.h"
#include "xtime_l.h"
#include <cmath>
#include <stdint.h>

#define INF_START_ENGINE_0_MASK 0b0001
#define INF_START_ENGINE_1_MASK 0b0010
#define INF_START_ENGINE_2_MASK 0b0100
#define INF_START_ENGINE_3_MASK 0b1000
#define INF_START_ENGINE_ALL    0b1111

using real_t = double;

typedef struct
{
	UINTPTR reg_base_addr; /**< Register base address */
	int fractional_bits;
} CFD_Model_t;

typedef struct
{
	real_t X[8];
} CFD_Input_t;

typedef struct
{
	real_t Y[4];
} CFD_Prediction_t;

static INLINE void CFD_Mdl_WriteHwReg(CFD_Model_t *pModel, size_t reg_index, int data)
{
	Xil_Out32(pModel->reg_base_addr + UINTPTR(reg_index * sizeof(int)), data);
}

static INLINE int CFD_Mdl_ReadHwReg(CFD_Model_t *pModel, size_t reg_index)
{
	return Xil_In32(pModel->reg_base_addr + UINTPTR(reg_index * sizeof(int)));
}

static INLINE real_t Q_to_real(int q, int frac_bits)
{
	return ((real_t)q) * std::pow(2.0, -(real_t)frac_bits);
}

static INLINE int real_to_Q(real_t q, int frac_bits)
{
	return (int)(q * std::pow(2.0, (real_t)frac_bits));
}

XStatus CFD_Init(CFD_Model_t *pModel);
XStatus CFD_SetInput(CFD_Model_t *pModel, CFD_Input_t *pInput, size_t engine_index, XTime* fp_q_conv, XTime* total_AXI);
XStatus CFD_StartPrediction(CFD_Model_t *pModel, uint8_t mask);
XStatus CFD_GetPredictionResult(CFD_Model_t *pModel, CFD_Prediction_t *pOut_pred, size_t engine_index, XTime* fp_q_conv, XTime* total_AXI);

/// @brief Reads the status register
int CFD_GetStatus(CFD_Model_t *pModel);

/// @brief Wait for a prediction given an engine index.
/// @note If launching multiple engines, waiting for any one of them will work.
void CFD_WaitForPrediction(CFD_Model_t *pModel, size_t engine_index);

