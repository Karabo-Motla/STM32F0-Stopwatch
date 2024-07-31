#include <stdio.h>
#include "stm32f0xx.h"
#include "lcd_stm32f0.h"

#define TRUE 1
#define FALSE 0

// global flag variables
volatile uint8_t startFlag = FALSE;
volatile uint8_t lapFlag = FALSE;
volatile uint8_t stopFlag = FALSE;
volatile uint8_t resetFlag = TRUE;

// Timer variables
volatile uint8_t minutes = 0;
volatile uint8_t seconds = 0;
volatile uint8_t hundredths = 0;



//Lap variables
volatile uint8_t lapMinutes = 0;
volatile uint8_t lapSeconds = 0;
volatile uint8_t lapHundredths = 0;


// Function prototypes
void initGPIO(void);
void initTIM14(void);
void checkPB(void);
void display(void);
void delay_ms(uint32_t milliseconds);
void convert2BCDASCII(const uint8_t min, const uint8_t sec, const uint8_t hund, char* resultPtr);

void initGPIO(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN; // Enable clock for GPIO port A

    // Set PB0, PB1, PB2, and PB3 as input for buttons
    GPIOA->MODER &= ~(GPIO_MODER_MODER0|GPIO_MODER_MODER1|GPIO_MODER_MODER2|GPIO_MODER_MODER3);
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR0_0|GPIO_PUPDR_PUPDR1_0|GPIO_PUPDR_PUPDR2_0|GPIO_PUPDR_PUPDR3_0); // Pull-up
    // GPIOB - LEDs
	// Enable the clock GPIOB
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	// Set PB0-B7, P10-PB11 to output mode
	GPIOB->MODER |= (GPIO_MODER_MODER0_0|GPIO_MODER_MODER1_0|GPIO_MODER_MODER2_0|GPIO_MODER_MODER3_0);
	
    // Turn all LEDs OFF x 
	GPIOB->ODR = 0;
}

void initTIM14(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN; // Enable clock for TIM14
    TIM14->PSC = 7;                     // Set prescaler to 7 (results in a division by 8)
    TIM14->ARR = 60000;                 // Set auto-reload to 60,000 for 0.01 s interval
    TIM14->DIER |= TIM_DIER_UIE;        // Enable update interrupt
    NVIC_EnableIRQ(TIM14_IRQn);         // Enable TIM14 interrupt in NVIC
    TIM14->CR1 |= TIM_CR1_CEN;          // Start timer
}


void TIM14_IRQHandler(void) {
    if (TIM14->SR & TIM_SR_UIF) {
        TIM14->SR &= ~TIM_SR_UIF; // Clear update interrupt flag
        if (startFlag && !stopFlag) { // Ensure we only increment if stopwatch is running and not stopped
            hundredths++;
            if (hundredths == 100) {
                hundredths = 0;
                seconds++;
                if (seconds == 60) {
                    seconds = 0;
                    minutes++;
                }
            }
        }
    }
}


void checkPB(void) {

    // Check each button separately and ensure we only act on press (not release)
    if (!((GPIOA->IDR) & GPIO_IDR_0)) {
        startFlag = TRUE;
        stopFlag = FALSE;
        resetFlag = FALSE;
        lapFlag = FALSE;
        GPIOB->ODR |= (1 << 0); // Turn on LED at PB0 for START
        GPIOB->ODR &= ~((1 << 1) | (1 << 2) | (1 << 3)); // Turn off LEDs at PB1, PB2, PB3
        lcd_command(CLEAR);
        lcd_putstring("Start Pressed");
    }
    if (!((GPIOA->IDR) & GPIO_IDR_1)) {
        lapFlag = TRUE;  // Set lapFlag to TRUE
        // Capture the current time as lap time
        lapMinutes = minutes;
        lapSeconds = seconds;
        lapHundredths = hundredths;
        GPIOB->ODR |= (1 << 1); // Turn on LED at PB1 for LAP
        GPIOB->ODR &= ~((1 << 0) | (1 << 2) | (1 << 3)); // Turn off LEDs at PB0, PB2, PB3

        lcd_command(CLEAR);
        char lapDisplay[20];
        snprintf(lapDisplay, sizeof(lapDisplay), "Lap %02d:%02d.%02d", lapMinutes, lapSeconds, lapHundredths);
        lcd_putstring(lapDisplay);
    }
    if (!((GPIOA->IDR) & GPIO_IDR_2)) {
        stopFlag = TRUE; // Stop incrementing
        startFlag = FALSE; // Make sure it doesn't start again
        lapFlag = FALSE; // Clear any lap flags

        GPIOB->ODR |= (1 << 2); // Turn on LED at PB2 for STOP
        GPIOB->ODR &= ~((1 << 0) | (1 << 1) | (1 << 3)); // Turn off LEDs at PB0, PB1, PB3

        lcd_command(CLEAR);
        char stopDisplay[20];
        snprintf(stopDisplay, sizeof(stopDisplay), "Stopped at %02d:%02d:%02d", minutes, seconds, hundredths);
        lcd_putstring(stopDisplay);
        delay_ms(50);
    }
    if (!((GPIOA->IDR) & GPIO_IDR_3)) {
        startFlag = FALSE;  // Stop the timer
        stopFlag = TRUE;  // Ensure the timer remains stopped
        lapFlag = FALSE;  // Clear the lapFlag if set
        resetFlag = TRUE;  // Indicate that a reset has occurred
        minutes = 0;
        seconds = 0;
        hundredths = 0;
        GPIOB->ODR |= (1 << 3); // Turn on LED at PB3 for RESET
        GPIOB->ODR &= ~((1 << 0) | (1 << 1) | (1 << 2)); // Turn off LEDs at PB0, PB1, PB2
        lcd_command(CLEAR);
        lcd_putstring("Reset Pressed");
        lcd_command(LINE_TWO);
        lcd_putstring("Press SW0...");
    }
    //lastButtonState = currentButtonState; // Update the last state
    delay_ms(50); // Debouncing delay
}


void delay_ms(uint32_t milliseconds) {
    for (uint32_t i = 0; i < milliseconds * 4000; i++) {
        __asm("nop"); // This will depend on your system clock for accurate timing
    }
}


void display(void) {
    char displayBuffer[20];  // Buffer for formatting strings

    // If the system has been reset
    if (resetFlag) {
        lcd_command(CLEAR);
        lcd_putstring("Stop Watch");
        lcd_command(LINE_TWO);
        lcd_putstring("Press SW0...");
    }
    // If the stopwatch is running
    else if (startFlag && !stopFlag) {
        // Convert current or lap time to BCD ASCII format
        convert2BCDASCII(minutes, seconds, hundredths, displayBuffer);
        lcd_command(CLEAR);
        if (lapFlag) {
            // Prefix with "Lap " if displaying lap time
            char lapDisplay[25];
            snprintf(lapDisplay, sizeof(lapDisplay), "Lap %s", displayBuffer);
            lcd_putstring(lapDisplay);
        } else {
            // Display running time
            lcd_putstring(displayBuffer);
        }
    }
    // If the stopwatch is stopped
    else if (stopFlag) {
        // Convert stopped time to BCD ASCII format
        convert2BCDASCII(minutes, seconds, hundredths, displayBuffer);
        lcd_command(CLEAR);
        char stopDisplay[25];
        snprintf(stopDisplay, sizeof(stopDisplay), "Stopped %s", displayBuffer);
        lcd_putstring(stopDisplay);
    }
}
void convert2BCDASCII(const uint8_t min, const uint8_t sec, const uint8_t hund, char* resultPtr) {
    // Convert decimal to BCD 
    uint8_t minBCD = (min / 10) * 16 + (min % 10);
    uint8_t secBCD = (sec / 10) * 16 + (sec % 10);
    uint8_t hundBCD = (hund / 10) * 16 + (hund % 10);

    // Format the BCD values into the resultPtr buffer as an ASCII string "mm:ss.hh"
    snprintf(resultPtr, 9, "%02X:%02X.%02X", minBCD, secBCD, hundBCD);
}


int main(void) {
    SystemInit();  // System Initialization
    init_LCD();    // Initialize the LCD
    initGPIO();    // Initialize GPIO
    initTIM14();   // Initialize TIM14


    delay_ms(1000);  // Initial delay for stabilization

    // Display initial message
    lcd_putstring("Stop Watch");
    lcd_command(LINE_TWO);
    lcd_putstring("Press SW0...");


    while (1) {
        checkPB();  // Check pushbuttons
        display();  // Update display
        delay_ms(100); // Delay to prevent rapid flickering
    }
    return 0; // Main should return an int
}

