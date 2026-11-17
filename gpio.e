.ifndef GPIO_E__INCLUDED
.equ GPIO_E__INCLUDED,1

.equ GPIO_BASE, 0x10012

.equ GPIO_INPUT_VAL,        0x00
.equ GPIO_INPUT_EN,         0x04
.equ GPIO_OUTPUT_EN,        0x08
.equ GPIO_OUTPUT_VAL,       0x0C
.equ GPIO_PUE,              0x10
.equ GPIO_DS,               0x14
.equ GPIO_RISE_IE,          0x18
.equ GPIO_RISE_IP,          0x1C
.equ GPIO_FALL_IE,          0x20
.equ GPIO_FALL_IP,          0x24
.equ GPIO_HIGH_IE,          0x28
.equ GPIO_HIGH_IP,          0x2C
.equ GPIO_LOW_IE,           0x30
.equ GPIO_LOW_IP,           0x34
.equ GPIO_IOF_EN,           0x38
.equ GPIO_IOF_SEL,          0x3C
.equ GPIO_OUT_XOR,          0x40
.equ GPIO_PASSTHRU_HIGH_IE, 0x44
.equ GPIO_PASSTHRU_LOW_IE,  0x48

.endif  # GPIO_E__INCLUDED
