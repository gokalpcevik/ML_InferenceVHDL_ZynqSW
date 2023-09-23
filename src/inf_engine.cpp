#include "inf_engine.h"

XStatus CFD_Init(CFD_Model_t *pModel)
{
    pModel->reg_base_addr = XPAR_CFD_MODEL_MREF_0_BASEADDR;
    pModel->fractional_bits = 15;
    return XST_SUCCESS;
}

XStatus CFD_SetInput(CFD_Model_t *pModel, CFD_Input_t *pInput, size_t engine_index)
{
    // * Convert to Q-format for hardware
    int Q_X[8];
    for (size_t i = 0; i < 8; i++)
        Q_X[i] = real_to_Q(pInput->X[i], pModel->fractional_bits);

    // * Input register indices-> 2 + engine_index * 12
    size_t reg_base_idx = 2 + engine_index * 12;

    // Registers 2 to 9 (inclusive) are the input registers.
    // ! Reverse because I messed up the register order in the hardware
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 7, Q_X[0]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 6, Q_X[1]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 5, Q_X[2]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 4, Q_X[3]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 3, Q_X[4]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 2, Q_X[5]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 1, Q_X[6]);
    CFD_Mdl_WriteHwReg(pModel, reg_base_idx + 0, Q_X[7]);

    return XST_SUCCESS;
}

XStatus CFD_StartPrediction(CFD_Model_t *pModel, uint8_t mask)
{
    CFD_Mdl_WriteHwReg(pModel, 0, mask);
    // Clear the control register
    CFD_Mdl_WriteHwReg(pModel, 0, 0x0);
    return XST_SUCCESS;
}

XStatus CFD_GetPredictionResult(CFD_Model_t *pModel, CFD_Prediction_t *pOut_pred, size_t engine_index)
{
    size_t reg_base_idx = 10 + engine_index * 12;

    pOut_pred->Y[0] = Q_to_real(CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 0), pModel->fractional_bits);
    pOut_pred->Y[1] = Q_to_real(CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 1), pModel->fractional_bits);
    pOut_pred->Y[2] = Q_to_real(CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 2), pModel->fractional_bits);
    pOut_pred->Y[3] = Q_to_real(CFD_Mdl_ReadHwReg(pModel, reg_base_idx + 3), pModel->fractional_bits);
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
