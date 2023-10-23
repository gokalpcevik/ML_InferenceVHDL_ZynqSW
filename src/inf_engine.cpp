#include "inf_engine.h"

XStatus CFD_Init(CFD_Model_t *pModel)
{
    pModel->reg_base_addr = XPAR_CFD_MODEL_MREF_0_BASEADDR;
    pModel->fractional_bits = 15;
    return XST_SUCCESS;
}

XStatus CFD_SetInput(CFD_Model_t *pModel, CFD_Input_t *pInput, size_t engine_index, XTime* fp_q_conv, XTime* total_AXI)
{
    XTime start, end;
    
    XTime_GetTime(&start);
    // * Convert to Q-format for hardware
    int Q_X[8];
    Q_X[0] = real_to_Q(pInput->X[0], pModel->fractional_bits);
    Q_X[1] = real_to_Q(pInput->X[1], pModel->fractional_bits);
    Q_X[2] = real_to_Q(pInput->X[2], pModel->fractional_bits);
    Q_X[3] = real_to_Q(pInput->X[3], pModel->fractional_bits);
    Q_X[4] = real_to_Q(pInput->X[4], pModel->fractional_bits);
    Q_X[5] = real_to_Q(pInput->X[5], pModel->fractional_bits);
    Q_X[6] = real_to_Q(pInput->X[6], pModel->fractional_bits);
    Q_X[7] = real_to_Q(pInput->X[7], pModel->fractional_bits);
    // * Input register indices-> 2 + engine_index * 12
    XTime_GetTime(&end);
    *fp_q_conv += end - start;

    size_t reg_base_idx = 2 + engine_index * 12;

    XTime_GetTime(&start);
    // Registers 2 to 9 (inclusive) are the input registers.
    // * Reverse because I messed up the register order in the hardware
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 7, Q_X[0]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 6, Q_X[1]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 5, Q_X[2]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 4, Q_X[3]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 3, Q_X[4]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 2, Q_X[5]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 1, Q_X[6]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 0, Q_X[7]);
    XTime_GetTime(&end);
    *total_AXI += end - start;

    return XST_SUCCESS;
}

XStatus CFD_StartPrediction(CFD_Model_t *pModel, uint8_t mask)
{
    CFD_Mdl_WriteHwReg(pModel, 0, mask);
    // Clear the control register
    CFD_Mdl_WriteHwReg(pModel, 0, 0x0);
    return XST_SUCCESS;
}

XStatus CFD_GetPredictionResult(CFD_Model_t *pModel, CFD_Prediction_t *pOut_pred, size_t engine_index, XTime* fp_q_conv, XTime* total_AXI)
{
    XTime start,end;

    size_t reg_base_idx = 10 + engine_index * 12;

    XTime_GetTime(&start);
    int Yhat0_hw = CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 0);
    int Yhat1_hw = CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 1);
    int Yhat2_hw = CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 2);
    int Yhat3_hw = CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 3);
    XTime_GetTime(&end);

    *total_AXI += end - start;

    XTime_GetTime(&start);
    pOut_pred->Y[0] = Q_to_real(Yhat0_hw, pModel->fractional_bits);
    pOut_pred->Y[1] = Q_to_real(Yhat1_hw, pModel->fractional_bits);
    pOut_pred->Y[2] = Q_to_real(Yhat2_hw, pModel->fractional_bits);
    pOut_pred->Y[3] = Q_to_real(Yhat3_hw, pModel->fractional_bits);
    XTime_GetTime(&end);
    *fp_q_conv += end - start;

    return XST_SUCCESS;
}

int CFD_GetStatus(CFD_Model_t *pModel)
{
    return CFD_Mdl_ReadHwReg(pModel, 1);
}

void CFD_WaitForPrediction(CFD_Model_t *pModel, size_t engine_index)
{
    while (CFD_Mdl_ReadHwReg(pModel, 1) & (1 << (engine_index * 2 + 1)))
        ;
}
