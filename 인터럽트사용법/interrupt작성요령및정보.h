
//가장 기본적인 Interrupt Service Routine 필요한것만여기서 Coding 하시면 됩니다.
static void s2mm_isr(void* CallbackRef)
{

	// Local variables
	u32      irq_status;
	int      time_out;
	XAxiDma* p_dma_inst = (XAxiDma*)CallbackRef;

	// Disable interrupts
	XAxiDma_IntrDisable(p_dma_inst, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrDisable(p_dma_inst, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

	// Read pending interrupts
	irq_status = XAxiDma_IntrGetIrq(p_dma_inst, XAXIDMA_DEVICE_TO_DMA);

	// Acknowledge pending interrupts
	XAxiDma_IntrAckIrq(p_dma_inst, irq_status, XAXIDMA_DEVICE_TO_DMA);

	// If no interrupt is asserted, we do not do anything
	if (!(irq_status & XAXIDMA_IRQ_ALL_MASK))
		return;

	// If error interrupt is asserted, raise error flag, reset the
	// hardware to recover from the error, and return with no further
	// processing.
	if ((irq_status & XAXIDMA_IRQ_ERROR_MASK))
	{
		g_dma_err = 1;
		// Reset should never fail for transmit channel
		XAxiDma_Reset(p_dma_inst);

		time_out = RESET_TIMEOUT_COUNTER;
		while (time_out)
		{
			if (XAxiDma_ResetIsDone(p_dma_inst))
				break;
			time_out -= 1;
		}
		return;
	}

	// Completion interrupt asserted
	if (irq_status & XAXIDMA_IRQ_IOC_MASK)
		g_s2mm_done = 1;

	// Re-enable interrupts
	XAxiDma_IntrEnable(p_dma_inst, (XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_ERROR_MASK), XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(p_dma_inst, (XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_ERROR_MASK), XAXIDMA_DEVICE_TO_DMA);

}

// Interrupt를 Interrupt Controller에 입력하는 과정
static int dma_connect_interrupt(void)
{
	//This functions sets up the interrupt on the Arm
	int status;

	XScuGic_Config *scugic_cfg = XScuGic_LookupConfig(Interrupt Controller ID를 안에 적으시오!); 
	//Scugic ID를 check하여 해당 ID가 있으면 그 ID data를 넘기는 것으로 알고있음
	if (scugic_cfg == NULL) {
		xil_printf("Interrupt Configuration Lookup Failed\n\r");
		return XST_FAILURE;
	}

	status = XScuGic_CfgInitialize(&scugic, scugic_cfg, scugic_cfg->CpuBaseAddress);
	// scugic 초기설정
	if(status != XST_SUCCESS) {
		return status;
	}

	status = XScuGic_SelfTest(&scugic);
	if(status != XST_SUCCESS) {
		return status;
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Multi로 사용할때는 이부분만 여러개 작성하면된다. 나머지 부분은 중복으로 작성할 필요없습니다.
	//해당 Interrupt ID에 Priority 설정 없어도 작동 되는 것으로 알고 있음 일부 Interrupt에 경우 없으면 발생안하니 주의
	XScuGic_SetPriorityTriggerType(&scugic, XPAR_FABRIC_AXIDMA_0_VEC_ID , 0xA0, 0x3); // Set priorit for multi interrupt
				
	status = XScuGic_Connect(&scugic, XPAR_FABRIC_AXIDMA_0_VEC_ID,(Xil_InterruptHandler) example_isr, &DMA0);
	//본인이 작성한 Handler를 해당 ID에 연동시키면서 해당 device도 call back 대상으로 등록 하는것으로 알고 있음
	if(status != XST_SUCCESS) {
		return status;
	}
	XScuGic_Enable(&scugic, XPAR_FABRIC_AXIDMA_0_VEC_ID);
	//해당 interrupt ID를 사용할수록 있도록 설정
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	Xil_ExceptionInit();
	// Exception에도 마찬가지로 XScugic를 연동시켜주어야 한다.
	// Scugic는 어떤 Interrupt를 사용할지를 정해주고 자신의 Handler를 시행하기 위해서는 Exception을 연동해야 사용가능 한것으로 알고 있음.
	// Interrupt 와 Exception을 Arm에서는 딱히 구분 짖지 않는 자료로보 IRQ Interrupt에 경우 5번째 Exception으로 기록되어있다.
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler) XScuGic_InterruptHandler, &scugic);

	Xil_ExceptionEnable();
		
	return XST_SUCCESS;
}

void main()
{
	////////////////////////////준비물
	1. device driver.h
	2. scugic관련 h파일
	3. excetption관련 h 파일도 include가 되어야하는데 xsugic.h 에 포함되어있는걸로 알고있다.
	4. xparameters.h 필요한 device ID , Interrupt ID , Interrupt Controller ID 다 여기에 적혀져 있다.
	
	//Interrupt 사용 방법
	1. 사용하고자 하는 Device를 Presetting 시켜준다. 
	2. Interrupt Connect 함수를 이용하여 연결하여 준다.
	3. Device에서 따로 Interrup_Enable을 해주어야한다.
	Device에는 기본적으로 controll register 와 status register 있다.
	이때 controll register에 12번째 bit (0번째 부터시작) 는 interrupt를 발생시키는 bit 이므로 default가 0 이므로 활성화가 필요합니다.
	error interrupt등 다양한 interrupt가 있는데 이런 interrupt에 관한 정보는 status register를 읽어서 알아낸다.
	ISR routine안에서 mask를 씌워서 읽는것 또한 status register을 읽어서 실행하는 것입니다. 
	
	//////////////////주의사항
	1. ISR을 너무 Heavy하게 작성하지 아니 하도록 한다.
	2. Global Variable을 통한 Flag 사용을 자제 하도록 한다.
	예를들어 ISR이 발생하면 isr_done이라는 variable을 1로 만드는 코드가 있고
	while(isr_done != 1) 이라는 코드가 있다고 가정하자.
	이 코드에 여러가지 문제를 소개해보면,
	1. 컴파일러는 Isr을 고려하지 않고 최적화를 하기때문에 while문안에 isr_done을 1을 만드는 조건이 없으면 while(1);과 같이 수행하도록 만든다.
	따라서 isr_done != 1을 Compiler 자체적으로 없앨 수 있다. 이를 해결하기위해 volatile을 사용하지만 또다른 문제가 있다.
	2. Isr을 실행하면 이전 pc값과 각종 register 정보를 백업시켜둔다. 그리고 service routine이 끝나면 원래 코드로 돌아가면서 다시백업된 정보를 덮어씌운다.
	이 과정에서 1 이 다시 0 으로 초기화 됨으로서 문제가 발생할 수 있다.
	
	이를 해결하기 위한 방법, device driver 과 같이 isr을 따로 c 파일로 빼서 작성하고 c파일안에 variable값을 읽어서 return 해주는 함수를 만들어 해결해야한다.
}


