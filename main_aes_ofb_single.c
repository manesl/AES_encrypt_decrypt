/* --COPYRIGHT--,BSD
 * Copyright (c) 2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/*******************************************************************************
 * MSP432 DMA - AES256 ECB Encryption/Decryption
 *
 * Description: In this code example, the DMA module is used to encrypt and
 * decrypt of block of data using the AES256 module. The ECB (electronic code
 * book) method of encryption/decryption is used in this example. A block of
 * 128-bits of data is fed into the AES256's DIN module by the use of the
 * MSP432's DMA module. While the data is being encrypted, the device is put
 * into LPM0 for power conservation. When the data encryption has finished,
 * the DMA and AES256 modules are reconfigured for decryption and the process
 * is repeated. At the end of the code, a no operation is inserted so that the
 * user can set a breakpoint and observe that the decrypted data is the same
 * as the original plaintext data.
 *
 *              MSP432P401
 *             ------------------
 *         /|\|                  |
 *          | |                  |
 *          --|RST               |
 *            |                  |
 *            |                  |
 *            |                  |
 *            |                  |
 *
 ******************************************************************************/

/* DriverLib Include */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

/* Standard Includes */
#include <stdint.h>
#include <string.h>

/* Static Variables */
static bool isFinished;

/* DMA Control Table */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(controlTable, 1024)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment=1024
#elif defined(__GNUC__)
__attribute__ ((aligned (1024)))
#elif defined(__CC_ARM)
__align(1024)
#endif
uint8_t controlTable[256];

/* AES Data and Cipher Key */
static uint8_t Data[16] =
{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

static uint8_t CipherKey[32] =
{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };

static uint8_t DataAESencrypted[16];       // Encrypted data
static uint8_t DataAESdecrypted[16];       // Decrypted data

int main(void)
{
    uint8_t i=0;
    /* Halting WDT  */
    MAP_WDT_A_holdTimer();
    MAP_Interrupt_disableMaster();

    /* Initializing the variables */
    isFinished = false;

    /* Restting the module, enabling AES DMA triggers, and setting to
     * encrypt mode (OFB) */
    //AES256->CTL0 = (AES256->CTL0 & ~(AES256_CTL0_CM_3 | AES256_CTL0_OP_3))
    //        | AES256_CTL0_CMEN;
    //AES256->CTL0 |= AES256_CTL0_SWRST; //AESSWRST
    AES256->CTL0 |= AES256_CTL0_CM_2; //OFB
    AES256->CTL0 |= AES256_CTL0_OP_0; //encryption
    AES256->CTL0 |= AES256_CTL0_CMEN; //AESCMEN
    //AES256->CTL0 &= ~AES256_CTL0_SWRST; //No reset
    
    /* Load a cipher key to module */
    MAP_AES256_setCipherKey(AES256_BASE, CipherKey, AES256_KEYLENGTH_256BIT);
    /* Write IV into AESAXIN */
    //for(i=0; i<8; i++){
        AES256->XIN=0x1111; //all 1's //half word=16 bits
    //}
    
    /* Configuring DMA module */
    MAP_DMA_enableModule();
    MAP_DMA_setControlBase(controlTable);

    /*
     * Primary DMA Channel, AES256
     * Size = 16 bits
     * Source Increment = 16 bits plaintext
     * Destination Increment = None AESAXIN
     * Arbitration = 8 , no other sources
     */
    MAP_DMA_setChannelControl(UDMA_PRI_SELECT | DMA_CH0_AESTRIGGER0,
            UDMA_SIZE_16 | UDMA_SRC_INC_16 | UDMA_DST_INC_NONE | UDMA_ARB_1);//single dma channel will do 8 half word transfers
    MAP_DMA_setChannelTransfer(UDMA_PRI_SELECT | DMA_CH0_AESTRIGGER0,
            UDMA_MODE_BASIC, (void*) Data, (void*) &AES256->XIN, 8);//no of data items, no of times this channel will be used->no of blocks*8 half word

    /*
     * Primary DMA Channel, AES256
     * Size = 16 bits
     * Source Increment = None AESADOUT
     * Destination Increment = 16 bits ciphertext
     * Arbitration = 8 , no other sources
     */
    MAP_DMA_setChannelControl(UDMA_PRI_SELECT | DMA_CH1_AESTRIGGER1,
            UDMA_SIZE_16 | UDMA_SRC_INC_NONE | UDMA_DST_INC_16 | UDMA_ARB_1);
    MAP_DMA_setChannelTransfer(UDMA_PRI_SELECT | DMA_CH1_AESTRIGGER1,
            UDMA_MODE_BASIC, (void*) &AES256->DOUT, (void*) DataAESencrypted, 8);
            
     /*
     * Primary DMA Channel, AES256
     * Size = 16 bits
     * Source Increment = 16 bits plaintext
     * Destination Increment = None AESAXDIN
     * Arbitration = 8 , no other sources
     */
    MAP_DMA_setChannelControl(UDMA_PRI_SELECT | DMA_CH2_AESTRIGGER2,
            UDMA_SIZE_16 | UDMA_SRC_INC_16 | UDMA_DST_INC_NONE | UDMA_ARB_1);
    MAP_DMA_setChannelTransfer(UDMA_PRI_SELECT | DMA_CH2_AESTRIGGER2,
            UDMA_MODE_BASIC, (void*) Data, (void*) &AES256->XDIN, 8);
            
    /* Assigning/Enabling Interrupts */
    MAP_DMA_assignInterrupt(DMA_INT1, 1); //this is for the final channel in use which will be channel 1
    MAP_Interrupt_enableInterrupt(INT_DMA_INT1);
    
    MAP_DMA_assignChannel(DMA_CH0_AESTRIGGER0);
    MAP_DMA_assignChannel(DMA_CH1_AESTRIGGER1);
    MAP_DMA_assignChannel(DMA_CH2_AESTRIGGER2);
    
    MAP_DMA_clearInterruptFlag(0);
    MAP_DMA_clearInterruptFlag(1);
    MAP_DMA_clearInterruptFlag(2);
    
    MAP_Interrupt_enableMaster();

    /* Enabling the DMA channels. Channel 1 is used for transfering the
     * plaintext data to the DIN register and Channel 0 is used to transfer the
     * encrypted data to an intermediary register.
     */
    
    MAP_DMA_enableChannel(2);
    MAP_DMA_enableChannel(0);
    MAP_DMA_enableChannel(1);
    /* Setting the number of 128-bit blocks to encrypt. Setting the CTL1
     * register will initiate the DMA transfer. Once all of the encryption
     * is completed, the DMA completion interrupt will be fired. */
    
    AES256->CTL1 = 1;
    AES256->STAT |= AES256_STAT_DINWR;
    /* Waiting for the DMA finished flag */
    while (!isFinished)
    {
        //MAP_PCM_gotoLPM0InterruptSafe();
    }

    /* Resetting the AES module */
    isFinished = false;

    /* Load a decipher key to module and resetting the module for decryption */
    /*AES256->CTL0 &= ~AES256_CTL0_CMEN;
    
    
    MAP_AES256_setDecipherKey(AES256_BASE, CipherKey, AES256_KEYLENGTH_256BIT);
    //AES256->CTL0 |= (AES256_CTL0_CMEN | AES256_CTL0_OP_3);
    //AES256->CTL0 |= AES256_CTL0_SWRST; //AESSWRST
    AES256->CTL0 |= AES256_CTL0_CM_2; //OFB
    AES256->CTL0 |= AES256_CTL0_OP_1; //decryption
    AES256->CTL0 |= AES256_CTL0_CMEN; //AESCMEN
    //AES256->CTL0 &= ~AES256_CTL0_SWRST; //No reset
    
    
    AES256->STAT |= AES256_STAT_KEYWR;*/
    AES256->CTL0 &= ~AES256_CTL0_CMEN;
    MAP_AES256_setDecipherKey(AES256_BASE, CipherKey, AES256_KEYLENGTH_256BIT);
    AES256->CTL0 |= (AES256_CTL0_CMEN | AES256_CTL0_OP_3 | AES256_CTL0_CM_2);
    AES256->STAT |= AES256_STAT_KEYWR;
    /* Write IV into AESAXIN */
    //for(i=0; i<8; i++){
    AES256->XIN=0x1111; //all 1's //half word=16 bits
    //}
    
    /*
     * Primary DMA Channel, AES256
     * Size = 16 bits
     * Source Increment = 16 bits ciphertext
     * Destination Increment = None AESAXIN
     * Arbitration = 8 , no other sources
     */
    MAP_DMA_setChannelControl(UDMA_PRI_SELECT | DMA_CH0_AESTRIGGER0,
            UDMA_SIZE_16 | UDMA_SRC_INC_16 | UDMA_DST_INC_NONE | UDMA_ARB_1);
    MAP_DMA_setChannelTransfer(UDMA_PRI_SELECT | DMA_CH0_AESTRIGGER0,
            UDMA_MODE_BASIC, (void*) DataAESencrypted, (void*) &AES256->XIN, 8);

    /*
     * Primary DMA Channel, AES256
     * Size = 16 bits
     * Source Increment = None AESADOUT
     * Destination Increment = 16 bits plaintext
     * Arbitration = 8 , no other sources
     */
    MAP_DMA_setChannelControl(UDMA_PRI_SELECT | DMA_CH1_AESTRIGGER1,
            UDMA_SIZE_16 | UDMA_SRC_INC_NONE | UDMA_DST_INC_16 | UDMA_ARB_1);
    MAP_DMA_setChannelTransfer(UDMA_PRI_SELECT | DMA_CH1_AESTRIGGER1,
            UDMA_MODE_BASIC, (void*) &AES256->DOUT, (void*) DataAESdecrypted, 8);      
            
    /*
     * Primary DMA Channel, AES256
     * Size = 16 bits
     * Source Increment = 16 bits ciphertext
     * Destination Increment = None AESAXDIN
     * Arbitration = 8 , no other sources
     */
    MAP_DMA_setChannelControl(UDMA_PRI_SELECT | DMA_CH2_AESTRIGGER2,
            UDMA_SIZE_16 | UDMA_SRC_INC_16 | UDMA_DST_INC_NONE | UDMA_ARB_1);
    MAP_DMA_setChannelTransfer(UDMA_PRI_SELECT | DMA_CH2_AESTRIGGER2,
            UDMA_MODE_BASIC, (void*) DataAESencrypted, (void*) &AES256->XDIN, 8); 
    /* Enabling the DMA channels. Channel 1 is used for transfering the
     * encrypted data to the DIN register and Channel 0 is used to transfer the
     * decrypted data to the decrypted data register */
    MAP_DMA_enableChannel(2);
    MAP_DMA_enableChannel(0);
    MAP_DMA_enableChannel(1);

    /* Setting the number of 128-bit blocks to decrypt. Setting the CTL1
     * register will initiate the DMA transfer. Once all of the decryption
     * is completed, the DMA completion interrupt will be fired. */
    AES256->CTL1 = 1;
    AES256->STAT |= AES256_STAT_DINWR;
    
    while (!isFinished)
    {
        //MAP_PCM_gotoLPM0InterruptSafe();
    }

    /* Set a breakpoint here. The data in DataAESdecrypted should math the
     * original data in Data.
     */
    __no_operation();
}

/* Completion interrupt for DMA */
void DMA_INT1_IRQHandler(void)
{
    MAP_DMA_disableChannel(0);
    MAP_DMA_disableChannel(1);
    MAP_DMA_disableChannel(2);
    isFinished = true;
}
