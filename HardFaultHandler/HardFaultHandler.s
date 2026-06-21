                AREA    |.text|, CODE, READONLY
                THUMB

                IMPORT  HardFaultHandler
                EXPORT  HardFault_Handler

HardFault_Handler
                tst     LR, #4
                ite     EQ
                mrseq   R0, MSP
                mrsne   R0, PSP
                b       HardFaultHandler

                ALIGN
                END