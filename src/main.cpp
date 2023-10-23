#include "xparameters.h"
#include "xstatus.h"
#include "xil_cache.h"
#include "xuartps.h"
#include "xtime_l.h"
#include "xscugic.h"
#include "sleep.h"
#include <stdio.h>


#include "inf_engine.h"
#include "test_in.h"

#define CFD_BAUD_RATE 921600
#define CFD_PERF 0
#define CFD_SEND 1

#define GET_MODEL() (CFD_Model_t *)&mdl

static volatile bool finished = true;

// CFD Model Prediction
static volatile CFD_Model_t mdl;
// Interrupt Driver Instance
static XScuGic intr_drv;
// PS UART Driver Instance
static XUartPs uart_drv;

/*
! >>> Steps for correctly setting up PL-PS interrupts <<<
1 - Config Initialize
2 - Self test

* Connect the interrupt controller interrupt handler to the hardware
* interrupt handling logic in the ARM processor.
3 - Xil_ExceptionRegisterHandler(.., XScuGic_InterruptHandler)

* Enable interrupts in the ARM
4 - Xil_ExceptionEnable

5 - Connect
6-  Set priority trigger type(rising edge, level high, etc...)
6 - Enable
*/

XStatus CfgInitInterrupts(void)
{
    XScuGic_Config *pCfg = XScuGic_LookupConfig(XPAR_SCUGIC_0_DEVICE_ID);
    if (pCfg == nullptr)
        return XST_FAILURE;

    int Status = XScuGic_CfgInitialize(&intr_drv, pCfg, pCfg->CpuBaseAddress);
    if (Status != XST_SUCCESS)
    {
        xil_printf("Interrupt controller init failed. \r\n");
        return XST_FAILURE;
    }

    Status = XScuGic_SelfTest(&intr_drv);
    if (Status != XST_SUCCESS)
    {
        xil_printf("Interrupt controller init failed. \r\n");
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

void CFD_Model_IntrHandler(void *data)
{
    finished = true;
}

void UART_InterruptHandler(void *CallBackRef, u32 Event, unsigned int EventData)
{
}

XStatus InitUartPs(void)
{
    XUartPs_Config *pCfg = XUartPs_LookupConfig(XPAR_PS7_UART_1_DEVICE_ID);
    XStatus status = XUartPs_CfgInitialize(&uart_drv, pCfg, pCfg->BaseAddress);
    if (status != XST_SUCCESS)
        return XST_FAILURE;
    status = XUartPs_SetBaudRate(&uart_drv, CFD_BAUD_RATE);

    return status;
}

XStatus SetupInterruptSystem(void)
{
    // Connect the interrupt controller interrupt handler to the hardware
    // interrupt handling logic in the ARM processor.
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, &intr_drv);

    // Enable IRQ interrupts in the ARM
    // ! Uncomment when using
    // Xil_ExceptionEnable();
    ;

    // Connect CFD intr. handler for GIC driver
    int Status = XScuGic_Connect(&intr_drv, XPAR_FABRIC_CFD_MODEL_AI_PRED_IRQ_INTR, (Xil_ExceptionHandler)CFD_Model_IntrHandler, (void *)NULL);

    if (Status != XST_SUCCESS)
        return Status;

    // Trigger on Rising Edge with highest prio
    XScuGic_SetPriorityTriggerType(&intr_drv, XPAR_FABRIC_CFD_MODEL_AI_PRED_IRQ_INTR, 0, 0b11);

    // Enable IRQ_F2P coming from PL fabric(CFD module)
    XScuGic_Enable(&intr_drv, XPAR_FABRIC_CFD_MODEL_AI_PRED_IRQ_INTR);

    // Connect UART interrupts
    Status = XScuGic_Connect(&intr_drv, XPAR_XUARTPS_1_INTR, (Xil_ExceptionHandler)XUartPs_InterruptHandler, (void *)&intr_drv);

    if (Status != XST_SUCCESS)
        return Status;

    XScuGic_Enable(&intr_drv, XPAR_XUARTPS_1_INTR);

    XUartPs_SetHandler(&uart_drv, (XUartPs_Handler)UART_InterruptHandler, &uart_drv);

    // Receive on TX FIFO Empty
    u32 UART_intr_mask = XUARTPS_IXR_TXEMPTY;

    XUartPs_SetInterruptMask(&uart_drv, UART_intr_mask);

    return Status;
}

#if CFD_PERF == 1
int main()
{
    // Init interrupt controller driver
    // int Status = CfgInitInterrupts();
    // if (Status != XST_SUCCESS)
    //    return XST_FAILURE;
    // Status = SetupInterruptSystem();
    // if (Status != XST_SUCCESS)
    //    return XST_FAILURE;

    /*********************************
     * SETUP CFD MODEL & PREDICTIONS *
     *********************************/
    int Status = CFD_Init((CFD_Model_t *)&mdl);
    if (Status != XST_SUCCESS)
    {
        xil_printf("CFD Initialization failed!.\r\n");
        return XST_FAILURE;
    };

    XTime total_axi = 0;
    XTime total_hw = 0;

    const size_t count = sizeof(test_inputs) / (sizeof(real_t) * 8);
    const size_t par_last_idx = count - count % 4;
    xil_printf(">> Started\r\n", par_last_idx);

    XTime start;
    XTime end;

    XTime_GetTime(&start);
    for (size_t x_idx = 0; x_idx < count - 4; x_idx += 4)
    {
        XTime start_axi;
        XTime end_axi;
        XTime start_hw;
        XTime end_hw;

        XTime_GetTime(&start_axi);
        CFD_Input_t *X0 = &test_inputs[x_idx + 0];
        CFD_Input_t *X1 = &test_inputs[x_idx + 1];
        CFD_Input_t *X2 = &test_inputs[x_idx + 2];
        CFD_Input_t *X3 = &test_inputs[x_idx + 3];
        CFD_SetInput(GET_MODEL(), X0, 0);
        CFD_SetInput(GET_MODEL(), X1, 1);
        CFD_SetInput(GET_MODEL(), X2, 2);
        CFD_SetInput(GET_MODEL(), X3, 3);
        XTime_GetTime(&end_axi);
        total_axi += end_axi - start_axi;

        XTime_GetTime((XTime *)&start_hw);
        CFD_StartPrediction(GET_MODEL(), INF_START_ENGINE_ALL);
        CFD_WaitForPrediction(GET_MODEL(), 0);
        XTime_GetTime((XTime *)&end_hw);
        total_hw += end_hw - start_hw;
    }
    XTime_GetTime(&end);

    xil_printf(">> Finished\r\n");
    xil_printf(">>>>>>>>>>>> Performance <<<<<<<<<<<<\r\n");
    printf(">> Total CPU Clock Cycles: %llu \r\n", 2 * (end - start));
    printf(">> Elapsed Time(CPU+HW+AXI): %.3fms \r\n", 1.0 * (end - start) / (COUNTS_PER_SECOND / 1000));
    xil_printf(">>>>>>>>>>>> Breakdown <<<<<<<<<<<<\r\n");
    printf(">> Elapsed Time(Hardware): %.3fms \r\n", 1.0 * (total_hw) / (COUNTS_PER_SECOND / 1000));
    printf(">> Elapsed Time(Memory/AXI-Lite): %.3fms \r\n", 1.0 * (total_axi) / (COUNTS_PER_SECOND / 1000));
    printf(">> Elapsed Time(CPU): %.3fms \r\n", 1.0 * (end - start - total_hw - total_axi) / (COUNTS_PER_SECOND / 1000));

    double total_time = (double)(end - start);
    double hw_per = (double)total_hw / total_time;
    double mem_per = (double)total_axi / total_time;
    double cpu_per = (double)(end - start - total_hw - total_axi) / total_time;
    double hw_to_mem_ratio = hw_per / mem_per;

    printf(">> Hardware(%%): %.3f \r\n", hw_per);
    printf(">> Memory(%%): %.3f \r\n", mem_per);
    printf(">> CPU(%%): %.3f \r\n", cpu_per);
    printf(">> Hardware-Memory ratio: %.3f \r\n", hw_per / mem_per);

    while (true)
    {
        usleep(100);
    }
    return XST_SUCCESS;
}

#elif CFD_SEND == 1

int main()
{
    s32 uart_res = InitUartPs();

    if (uart_res != XST_SUCCESS)
    {
        xil_printf("error\r\n");
    }

    /*********************************
     * SETUP CFD MODEL & PREDICTIONS *
     *********************************/
    int Status = CFD_Init((CFD_Model_t *)&mdl);
    if (Status != XST_SUCCESS)
    {
        xil_printf("CFD Initialization failed!.\r\n");
        return XST_FAILURE;
    }

    XTime start;
    XTime end;
    XTime total_axi = 0;
    XTime total_hw = 0;
    XTime fp_q_conv = 0;

    const size_t count = sizeof(test_inputs) / (sizeof(real_t) * 8);

    CFD_Prediction_t *predictions = new CFD_Prediction_t[count];

    xil_printf(">>>> Started predictions.(Count=%llu)\r\n", count);
    XTime_GetTime(&start);
    for (size_t x_idx = 0; x_idx < count - 4; x_idx += 4)
    {
        XTime start_hw;
        XTime end_hw;

        CFD_Input_t *X0 = &test_inputs[x_idx + 0];
        CFD_Input_t *X1 = &test_inputs[x_idx + 1];
        CFD_Input_t *X2 = &test_inputs[x_idx + 2];
        CFD_Input_t *X3 = &test_inputs[x_idx + 3];
        CFD_SetInput(GET_MODEL(), X0, 0, &fp_q_conv, &total_axi);
        CFD_SetInput(GET_MODEL(), X1, 1, &fp_q_conv, &total_axi);
        CFD_SetInput(GET_MODEL(), X2, 2, &fp_q_conv, &total_axi);
        CFD_SetInput(GET_MODEL(), X3, 3, &fp_q_conv, &total_axi);

        XTime_GetTime(&start_hw);
        CFD_StartPrediction(GET_MODEL(), INF_START_ENGINE_ALL);
        CFD_WaitForPrediction(GET_MODEL(), 0);
        XTime_GetTime(&end_hw);
        total_hw += end_hw - start_hw;

        CFD_GetPredictionResult(GET_MODEL(), &predictions[x_idx + 0], 0, &fp_q_conv, &total_axi);
        CFD_GetPredictionResult(GET_MODEL(), &predictions[x_idx + 1], 1, &fp_q_conv, &total_axi);
        CFD_GetPredictionResult(GET_MODEL(), &predictions[x_idx + 2], 2, &fp_q_conv, &total_axi);
        CFD_GetPredictionResult(GET_MODEL(), &predictions[x_idx + 3], 3, &fp_q_conv, &total_axi);
    }
    XTime_GetTime(&end);

    xil_printf(">>>> Finished predictions.\r\n");
    xil_printf(">>>>>>>>>>>> Performance <<<<<<<<<<<<\r\n");
    printf(">>>> Total CPU Clock Cycles: %llu \r\n", 2 * (end - start));
    printf(">>>> Total Elapsed Time: %.3fms \r\n", 1.0 * (end - start) / (COUNTS_PER_SECOND / 1000));
    xil_printf(">>>>>>>>>>>> Breakdown <<<<<<<<<<<<\r\n");
    printf(">>>> Elapsed Time(Hardware): %.3fms \r\n", 1.0 * (total_hw) / (COUNTS_PER_SECOND / 1000));
    printf(">>>> Elapsed Time(Memory/AXI-Lite): %.3fms \r\n", 1.0 * (total_axi) / (COUNTS_PER_SECOND / 1000));
    printf(">>>> Elapsed Time(FP To/From Q): %.3fms \r\n", 1.0 * (fp_q_conv) / (COUNTS_PER_SECOND / 1000));
    printf(">>>> Elapsed Time(Other): %.3fms \r\n", 1.0 * (end - start - total_hw - total_axi - fp_q_conv) / (COUNTS_PER_SECOND / 1000));

    UINTPTR const uart_base_addr = uart_drv.Config.BaseAddress;
    size_t const bytes_to_send_per_pred = sizeof(real_t) * 4;
    size_t sent_data_size_bytes = 0;

    xil_printf("Starting transmission.\r\n");

    for (size_t outer_tx_idx = 0; outer_tx_idx < count - 4; outer_tx_idx += 4)
    {
        u8 start = '+';
        XUartPs_SendByte(uart_base_addr, start);
        XUartPs_SendByte(uart_base_addr, start);

        for (size_t inner_tx_idx = 0; inner_tx_idx < 4; inner_tx_idx++)
        {
            while (!XUartPs_IsTransmitEmpty(&uart_drv));
            u8 *pred_buf = (u8 *)&predictions[outer_tx_idx + inner_tx_idx];
            
            u32 total_bytes_sent = 0;
            while (total_bytes_sent != bytes_to_send_per_pred)
            {
                u32 rem_bytes = bytes_to_send_per_pred - total_bytes_sent;
                u32 bytes_sent = XUartPs_Send(&uart_drv, &pred_buf[bytes_to_send_per_pred - rem_bytes], rem_bytes);
                total_bytes_sent += bytes_sent;
            }
        }
    }

    xil_printf(">>>> Finished transmission.\r\n");

    // Terminate polling in PC
    u8 terminate = '!';
    XUartPs_SendByte(uart_drv.Config.BaseAddress, terminate);
    XUartPs_SendByte(uart_drv.Config.BaseAddress, terminate);

    while (true)
    {
        usleep(100);
    }
    return XST_SUCCESS;
}

#endif
