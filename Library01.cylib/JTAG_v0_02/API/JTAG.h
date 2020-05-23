/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#if !defined(JTAG_`$INSTANCE_NAME`_H)
#define JTAG_`$INSTANCE_NAME`_H

#include "`$INSTANCE_NAME`_defs.h"

#define MSB_FIRST (0)
#define LSB_FIRST (1)

#define `$INSTANCE_NAME`_OutBits `$INSTANCE_NAME`_Datapath_1_F0_REG
#define `$INSTANCE_NAME`_InBits `$INSTANCE_NAME`_Datapath_1_F1_REG

#define `$INSTANCE_NAME`_Cmd `$INSTANCE_NAME`_CtrlReg_1_Control
#define `$INSTANCE_NAME`_Cmd_Write(cmd) `$INSTANCE_NAME`_CtrlReg_1_Write(cmd) 
#define `$INSTANCE_NAME`_Cmd_Read() `$INSTANCE_NAME`_CtrlReg_1_Read() 

#define `$INSTANCE_NAME`_Stat `$INSTANCE_NAME`_StatusReg_1_Status
#define `$INSTANCE_NAME`_Stat_Read() `$INSTANCE_NAME`_StatusReg_1_Read()

void `$INSTANCE_NAME`_Start(void);
void `$INSTANCE_NAME`_Reset(void);
void `$INSTANCE_NAME`_Set_Shift_Dir(uint8 dir);
uint8 `$INSTANCE_NAME`_Get_Shift_Dir(void);
void `$INSTANCE_NAME`_TAP_Reset(void);
void `$INSTANCE_NAME`_TAP_Move(uint8 new_state);
uint8 `$INSTANCE_NAME`_TAP_Get_State(void);
uint8 `$INSTANCE_NAME`_TAP_Scan(uint8 count, uint8 out_bits, uint8 last_tms);

#endif

/* [] END OF FILE */
