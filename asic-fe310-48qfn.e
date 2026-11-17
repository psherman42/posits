.ifndef ASIC_FE310_48QFN_E__INCLUDED
.equ ASIC_FE310_48QFN_E__INCLUDED,1

#
# ASIC PACKAGE pins
#   consult SoC datasheet for this information
#
.equ FE310_48QFN_UNKN,                      (0)          # non-existent pin
.equ FE310_48QFN25_GPIO0_PWM0_0,            (1UL <<  0)
.equ FE310_48QFN26_GPIO1_PWM0_1,            (1UL <<  1)
.equ FE310_48QFN27_GPIO2_PWM0_2_SPI1_SS0,   (1UL <<  2)
.equ FE310_48QFN28_GPIO3_PWM0_3_SPI1_MOSI,  (1UL <<  3)
.equ FE310_48QFN29_GPIO4_SPI1_MISO,         (1UL <<  4)
.equ FE310_48QFN31_GPIO5_SPI1_SCK,          (1UL <<  5)
.equ FE310_48QFN33_GPIO9_SPI1_SS2,          (1UL <<  9)
.equ FE310_48QFN34_GPIO10_PWM2_0_SPI1_SS3,  (1UL << 10)
.equ FE310_48QFN35_GPIO11_PWM2_1,           (1UL << 11)
.equ FE310_48QFN36_GPIO12_PWM2_2,           (1UL << 12)
.equ FE310_48QFN37_GPIO13_PWM2_3,           (1UL << 13)
.equ FE310_48QFN38_GPIO16_UART0_RX,         (1UL << 16)
.equ FE310_48QFN39_GPIO17_UART0_TX,         (1UL << 17)
.equ FE310_48QFN40_GPIO18_UART1_TX,         (1UL << 18)
.equ FE310_48QFN42_GPIO20_PWM1_0,           (1UL << 20)
.equ FE310_48QFN41_GPIO19_PWM1_1,           (1UL << 19)
.equ FE310_48QFN43_GPIO21_PWM1_2,           (1UL << 21)
.equ FE310_48QFN44_GPIO22_PWM1_3,           (1UL << 22)
.equ FE310_48QFN45_GPIO23_UART1_RX,         (1UL << 23)

.endif  # ASIC_FE310_48QFN_E__INCLUDED
