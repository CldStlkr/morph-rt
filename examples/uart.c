#include "stm32f4xx.h" // The CMSIS device header for register definitions

/* -------------------------------------------------------------------------
 * STEP 1: INITIALIZE THE REGISTERS
 * ------------------------------------------------------------------------- */
void USART2_Init_BareMetal(void) {
  // 1. Turn on the silicon clocks for the GPIOA port and the USART2 peripheral
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  // 2. Configure PA2 and PA3 as "Alternate Function" (AF) instead of standard
  // input/output This tells the silicon matrix to route these physical pins
  // directly to the USART hardware.

  // Clear the mode bits for pins 2 and 3, then set them to '10' (Alternate
  // Function)
  GPIOA->MODER &= ~((3 << (2 * 2)) | (3 << (2 * 3)));
  GPIOA->MODER |= ((2 << (2 * 2)) | (2 << (2 * 3)));

  // Set the specific Alternate Function to AF7 (which is USART2 for pins
  // PA2/PA3)
  GPIOA->AFR[0] &= ~((0xF << (4 * 2)) | (0xF << (4 * 3)));
  GPIOA->AFR[0] |= ((0x7 << (4 * 2)) | (0x7 << (4 * 3)));

  // 3. Configure the Protocol and Baud Rate (115200)
  // NOTE: This Baud Rate Register calculation assumes your STM32 is running on
  // its default 16 MHz internal clock (HSI) at boot. If your Morph-RT C code
  // has a SystemClock_Config() that boosts the APB1 clock to 42MHz, this value
  // will need to be recalculated!
  USART2->BRR = 0x008B; // (Mantissa = 8, Fraction = 11) for 16MHz clock

  // 4. Enable the USART (UE), the Transmitter (TE), and the Receiver (RE)
  USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/* -------------------------------------------------------------------------
 * STEP 2: HARDWARE POLLING FUNCTIONS
 * ------------------------------------------------------------------------- */
char USART2_Read(void) {
  // Block the CPU and spin endlessly until the physical RXNE (Receive Not
  // Empty) flag is set to 1
  while (!(USART2->SR & USART_SR_RXNE))
    ;

  // Reading the Data Register clears the RXNE flag automatically
  return USART2->DR;
}

void USART2_Write(char data) {
  // Block the CPU and spin endlessly until the TXE (Transmit Empty) flag is 1
  while (!(USART2->SR & USART_SR_TXE))
    ;

  // Writing to the Data Register automatically starts the physical transmission
  USART2->DR = data;
}

/* -------------------------------------------------------------------------
 * STEP 3: THE ECHO LOOP
 * ------------------------------------------------------------------------- */
int main(void) {
  USART2_Init_BareMetal();

  // The infinite event loop
  while (1) {
    // The CPU hangs here completely frozen until a byte arrives over the wire
    char incoming_byte = USART2_Read();

    // Bounce it right back!
    USART2_Write(incoming_byte + 1);
  }

  return 0;
}
