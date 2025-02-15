#include <msp430.h> 

#define XT2_FREQ   4000000
#define MCLK_FREQ 16000000
#define SMCLK_FREQ 4000000


void light_off(void) {
    P8OUT &= ~ 0x02;
    P3OUT &= ~ 0x80;
    P7OUT &= ~ 0x10;
    P6OUT &= ~ 0x08;
    P6OUT &= ~ 0x10;
    P3OUT &= ~ 0x20;
}

void led_on(void) {
    P1OUT |= BIT5;
    P2OUT  &= ~ BIT5;
}

void led_off(void) {
    P1OUT &= ~ BIT5;
    P2OUT |= BIT5;
}

void initGPIO(void) {
    P1DIR &= ~ 0x0C;
    P1REN |= 0x0C;
    P1OUT |= 0x0C;

    P1IE |= 0x0C;
    P1IES |= 0x0C;
    P1IFG &= ~ 0x0C;
}

void initLED(void) {
    P8DIR |= 0x02;
    P3DIR |= 0x80;
    P7DIR |= 0x10;
    P6DIR |= 0x08;
    P6DIR |= 0x10;
    P3DIR |= 0x20;

    light_off();
}

void initIO()
{
    P6DIR &= ~ 0x03;          // 配置GPIO引脚P6.0、P6.1为输出方向
    P3DIR |= 0xA0;            // 配置GPIO引脚P3.5(TB0.5)、P3.7(TB0OUTH/SVMOUT)为输出方向
    P7DIR |= 0x10;            // 配置GPIO引脚P7.4(TB0.2)为输出方向

    P1DIR |= BIT5;
    P2DIR |= BIT4 + BIT5;     // P2.4 and P2.5 output
    P2SEL |= BIT4 + BIT5;     // P2.4 and P2.5 options select
    led_on();

    initLED();
    initGPIO();
}

void initClock()
{
     UCSCTL6 &= ~ XT1OFF;                           // 启动XT1
     P5SEL |= BIT2 + BIT3;                          // XT2引脚功能选择
     UCSCTL6 &= ~ XT2OFF;                           // 打开XT2
     _bis_SR_register(SCG0);
     UCSCTL0 = DCO0 + DCO1 + DCO2 + DCO3 + DCO4;
     UCSCTL1 = DCORSEL_4;                           // DCO频率范围在28.2MHZ以下
     UCSCTL2 = FLLD_5 + 1;                          // D=16，N=1
     UCSCTL3 = SELREF_5 + FLLREFDIV_3;              // n=8,FLLREFCLK时钟源为XT2CLK；DCOCLK=D*(N+1)*(FLLREFCLK/n);DCOCLKDIV=(N+1)*(FLLREFCLK/n);
     UCSCTL4 = SELA_4 + SELS_3 + SELM_3;            // ACLK的时钟源为DCOCLKDIV,MCLK\SMCLK的时钟源为DCOCLK
     UCSCTL5 = DIVA_5 + DIVS_1;                     // ACLK由DCOCLKDIV的32分频得到，SMCLK由DCOCLK的2分频得到
                                                    // 最终MCLK:16MHZ,SMCLK:8MHZ,ACLK:32KHZ
//   __bic_SR_register(SCG0);                       // Enable the FLL control loop
}

void lightCtrl(volatile unsigned value) {
    light_off();

    switch (value) {
        case 5:
            P3OUT |= 0x20;
        case 4:
            P6OUT |= 0x10;
        case 3:
            P6OUT |= 0x08;
            led_on();                               // 重新开大功率LED
        case 2:
            P7OUT |= 0x10;
            WDTCTL = 0x5A3B;                        // 喂狗
        case 1:
            P3OUT |= 0x80;
        case 0:
            P8OUT |= 0x02;
            break;
        default:
            break;
    }
}

void ledCtrl(volatile unsigned duty) {
    TA2CCR0 = 500;                                  // PWM Period
    TA2CCTL1 = OUTMOD_6;                            // CCR1 toggle/set
    TA2CCR1 = 5 * duty;                             // CCR1 PWM duty cycle
    TA2CCTL2 = OUTMOD_6;                            // CCR2 toggle/set
    TA2CCR2 = 0;                                    // CCR2 PWM duty cycle
    TA2CTL = TASSEL_2 + MC_3 + TACLR;               // SMCLK, up-down mode, clear TAR
}


void ADC12_SCSC(void) {
    ADC12CTL0 |= ADC12SHT03 + ADC12MSC + ADC12ON;
    ADC12CTL1 |= ADC12SHP + ADC12CONSEQ_3 + ADC12CSTARTADD_0;
    ADC12MCTL0 |= ADC12INCH_0;                                   // 麦克风
    ADC12MCTL1 |= ADC12INCH_1 + ADC12EOS;                        // 光敏电阻
    //ADC12MCTL2 |= ADC12INCH_5 + ADC12EOS;                      // 拨盘
    //ADC12IE = 0x0007;
    ADC12IE = 0x0003;
    ADC12CTL0 |= ADC12ENC + ADC12SC;
}

int main(void)
{
    _enable_interrupt();
    WDTCTL = 0x5A3B;                                             // 设置看门狗16秒
    SFRIE1 |= WDTIE;

    initClock();
    initIO();

    ADC12_SCSC();
    return 0;
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1_Key(void) {
    if (P1IFG & 0x04) {
        led_on();
        P1IFG &= ~ 0x04;
    }
    if (P1IFG & 0x08) {
        led_off();
        P1IFG &= ~ 0x08;
    }
}

#pragma vector= WDT_VECTOR
__interrupt void watchdog_timer(void) {
    led_off();                                      // 超时关灯
}

#pragma vector = ADC12_VECTOR
__interrupt void ADC12(void) {
    volatile unsigned int value_buf;

    switch(ADC12IV) {                               // 判断中断向量
        case 6:                                     // 0x0006为MEM0
            value_buf = ADC12MEM0;                  // 麦克风
            lightCtrl((value_buf - 1800) / 400);
            //ledCtrl((value_buf - 1800) / 400);
            break;
        case 8:                                     // 0x0008为MEM1
            value_buf = ADC12MEM1;                  // 光敏电阻
            //lightCtrl(value_buf / 700);
            ledCtrl(value_buf / 2000);
            break;
//      case 10:                                    // 0x0008为MEM2
//          value_buf = ADC12MEM2;                  // 拨盘
//          break;
        default:
            break;
    }
}
