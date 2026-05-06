#include "fsl_debug_console.h"
#include "board.h"
#include "math.h"
#include "fsl_mma.h"
#include "fsl_tpm.h"

#include "fsl_common.h"
#include "pin_mux.h"
#include "fsl_gpio.h"
#include "fsl_port.h"

#include <MKL46Z4.h>
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BOARD_TIMER_BASEADDR TPM0
#define BOARD_FIRST_TIMER_CHANNEL 5U
#define BOARD_SECOND_TIMER_CHANNEL 2U
/* Get source clock for TPM driver */
#define BOARD_TIMER_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_BusClk)
#define TIMER_CLOCK_MODE 1U
/* I2C source clock */
#define I2C_BAUDRATE 100000U

#define I2C_RELEASE_SDA_PORT PORTE
#define I2C_RELEASE_SCL_PORT PORTE
#define I2C_RELEASE_SDA_GPIO GPIOE
#define I2C_RELEASE_SDA_PIN 25U
#define I2C_RELEASE_SCL_GPIO GPIOE
#define I2C_RELEASE_SCL_PIN 24U
#define I2C_RELEASE_BUS_COUNT 100U
/* Upper bound and lower bound angle values */
#define ANGLE_UPPER_BOUND 80 
#define ANGLE_LOWER_BOUND 30 

/*******************************************************************************
 * Prototypes: See Utility Documentation and Code section for definitions
 ******************************************************************************/
void BOARD_I2C_ReleaseBus(void);
void delay_ms(uint32_t ms);

// LED functions
void init_leds_pins(void);
void startup_light(void);

void red_on(void);
void red_off(void);
void red_toggle(void);
void green_on(void);
void green_off(void);
void green_toggle(void);

// Switch functions
void init_switch(uint32_t switch_pin);
void init_switch_pins(void);

// Duty cycle update function
void update_xDuty_cycle(int16_t xAngle, int16_t *xDuty_p);
void handle_switch_inputs(void);
void clear_interrupt(uint32_t sw_pin);

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* MMA8451 device address */
const uint8_t g_accel_address[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};

/* Switches and LEDs pins*/
const int led_pin_red = 16;
const int led_pin_green = 6;
const int switch_pin_1 = 3;
const int switch_pin_2 = 12;

/*******************************************************************************
 * Default Bubble Code
 ******************************************************************************/
static void i2c_release_bus_delay(void)
{
    uint32_t i = 0;
    for (i = 0; i < I2C_RELEASE_BUS_COUNT; i++)
    {
        __NOP();
    }
}

void BOARD_I2C_ReleaseBus(void)
{
    uint8_t i = 0;
    gpio_pin_config_t pin_config;
    port_pin_config_t i2c_pin_config = {0};

    /* Config pin mux as gpio */
    i2c_pin_config.pullSelect = kPORT_PullUp;
    i2c_pin_config.mux = kPORT_MuxAsGpio;

    pin_config.pinDirection = kGPIO_DigitalOutput;
    pin_config.outputLogic = 1U;
    CLOCK_EnableClock(kCLOCK_PortE);
    PORT_SetPinConfig(I2C_RELEASE_SCL_PORT, I2C_RELEASE_SCL_PIN, &i2c_pin_config);
    PORT_SetPinConfig(I2C_RELEASE_SDA_PORT, I2C_RELEASE_SDA_PIN, &i2c_pin_config);

    GPIO_PinInit(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, &pin_config);
    GPIO_PinInit(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, &pin_config);

    /* Drive SDA low first to simulate a start */
    GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 0U);
    i2c_release_bus_delay();

    /* Send 9 pulses on SCL and keep SDA high */
    for (i = 0; i < 9; i++)
    {
        GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 0U);
        i2c_release_bus_delay();

        GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 1U);
        i2c_release_bus_delay();

        GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 1U);
        i2c_release_bus_delay();
        i2c_release_bus_delay();
    }

    /* Send stop */
    GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(I2C_RELEASE_SCL_GPIO, I2C_RELEASE_SCL_PIN, 1U);
    i2c_release_bus_delay();

    GPIO_PinWrite(I2C_RELEASE_SDA_GPIO, I2C_RELEASE_SDA_PIN, 1U);
    i2c_release_bus_delay();
}
/* Initialize timer module */
static void Timer_Init(void)
{
    /* convert to match type of data */
    tpm_config_t tpmInfo;
    tpm_chnl_pwm_signal_param_t tpmParam[2];

    /* Configure tpm params with frequency 24kHZ */
    tpmParam[0].chnlNumber = (tpm_chnl_t)BOARD_FIRST_TIMER_CHANNEL;
    tpmParam[0].level = kTPM_LowTrue;
    tpmParam[0].dutyCyclePercent = 0U;

    tpmParam[1].chnlNumber = (tpm_chnl_t)BOARD_SECOND_TIMER_CHANNEL;
    tpmParam[1].level = kTPM_LowTrue;
    tpmParam[1].dutyCyclePercent = 0U;

    /* Initialize TPM module */
    TPM_GetDefaultConfig(&tpmInfo);
    TPM_Init(BOARD_TIMER_BASEADDR, &tpmInfo);

    CLOCK_SetTpmClock(1U);

    TPM_SetupPwm(BOARD_TIMER_BASEADDR, tpmParam, 2U, kTPM_EdgeAlignedPwm, 24000U, BOARD_TIMER_SOURCE_CLOCK);
    TPM_StartTimer(BOARD_TIMER_BASEADDR, kTPM_SystemClock);
}

/* Update the duty cycle of an active pwm signal */
static void Board_UpdatePwm(uint16_t x, uint16_t y)
{
    /* Updated duty cycle */
    TPM_UpdatePwmDutycycle(BOARD_TIMER_BASEADDR, (tpm_chnl_t)BOARD_FIRST_TIMER_CHANNEL, kTPM_EdgeAlignedPwm, x);
    TPM_UpdatePwmDutycycle(BOARD_TIMER_BASEADDR, (tpm_chnl_t)BOARD_SECOND_TIMER_CHANNEL, kTPM_EdgeAlignedPwm, y);
}

/*******************************************************************************
 * Main Function
 ******************************************************************************/
int main(void)
{
    init_leds_pins(); // Initialize LED pins
    init_switch_pins();  // Initialize switch pins

    mma_handle_t mmaHandle = {0};
    mma_data_t sensorData = {0};
    mma_config_t config = {0};
    status_t result;
    uint8_t sensorRange = 0;
    uint8_t dataScale = 0;
    int16_t xData = 0;
    int16_t xAngle = 0;
    int16_t xDuty = 0;
    uint8_t i = 0;
    uint8_t array_addr_size = 0;

    /* Board pin, clock, debug console init */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_I2C_ReleaseBus();
    BOARD_I2C_ConfigurePins();
    BOARD_InitDebugConsole();

    /* I2C initialize */
    BOARD_Accel_I2C_Init();
    /* Configure the I2C function */
    config.I2C_SendFunc = BOARD_Accel_I2C_Send;
    config.I2C_ReceiveFunc = BOARD_Accel_I2C_Receive;

    /* Initialize sensor devices */
    array_addr_size = sizeof(g_accel_address) / sizeof(g_accel_address[0]);
    for (i = 0; i < array_addr_size; i++)
    {
        config.slaveAddress = g_accel_address[i];
        /* Initialize accelerometer sensor */
        result = MMA_Init(&mmaHandle, &config);
        if (result == kStatus_Success)
        {
            break;
        }
    }

    if (result != kStatus_Success)
    {
        PRINTF("\r\nSensor device initialize failed!\r\n");
        return -1;
    }
    /* Get sensor range */
    if (MMA_ReadReg(&mmaHandle, kMMA8451_XYZ_DATA_CFG, &sensorRange) != kStatus_Success)
    {
        return -1;
    }
    if (sensorRange == 0x00)
    {
        dataScale = 2U;
    }
    else if (sensorRange == 0x01)
    {
        dataScale = 4U;
    }
    else if (sensorRange == 0x10)
    {
        dataScale = 8U;
    }
    else
    {
    }
    /* Init timer */
    Timer_Init();

    // Print message on terminal to indicate game start
    PRINTF("ASCII RACER: Controller Reset and Initialized.\n");
    PRINTF("asciiracer\n"); // Triggers the game if script is running in background 
    startup_light(); // Start up lighting sequence

    // Polling loop to update state based on accelerometer data and switch inputs
    while (1)
    {
        handle_switch_inputs(); // Check for switch inputs and perform actions

        if (MMA_ReadSensorData(&mmaHandle, &sensorData) != kStatus_Success) // Read sensor data
        {
            return -1;
        }

        xData = (int16_t)((uint16_t)((uint16_t)sensorData.accelXMSB << 8) | (uint16_t)sensorData.accelXLSB) / 4U; // Get the accelerometer data

        xAngle = (int16_t)floor((double)xData * (double)dataScale * 90 / 8192); // Calculate the tilt angle based on the accelerometer data

        update_xDuty_cycle(xAngle, &xDuty); // Update duty cycle influencing LED brightness and car movement.

        Board_UpdatePwm(xDuty, 0); // Update the game's speedometer and LED light's brightness based on the calculated duty cycle
    }
}

/*******************************************************************************
 * Utility Documentation and Code
 ******************************************************************************/

/**
 * @brief Executes a startup lighting sequence to indicate start of game and race.
 * 
 * The lighting sequence involves the red and green LEDs.
 * Each LED is turned on and then off with a delay of 2000 ms in between.
 */
void startup_light(void)
{
    red_on();
    delay_ms(2000);
    red_off();
    delay_ms(2000);
    red_on();
    delay_ms(2000);
    red_off();
    delay_ms(2000);
    green_on();
    delay_ms(2000);
    green_off();
}

/**
 * @brief Delays program execution for a specified number of milliseconds.
 * 
 * @param ms The duration of the delay in milliseconds.
 */
void delay_ms(uint32_t ms)
{
    uint32_t count = 0;
    const uint32_t delayCount = 1000 * ms;

    while (count < delayCount)
    {
        __NOP();  
        count++;
    }
}

/**
 * @brief Initializes the GPIO pins for controlling the LEDs.
 * 
 * The function configures the red and green LED pins on Port E and Port D
 * as output and sets them to their default state (off).
 */
void init_leds_pins(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK | SIM_SCGC5_PORTD_MASK;  

    PORTE->PCR[led_pin_red] = PORT_PCR_MUX(0b001);
    PTE->PSOR = 1 << led_pin_red;  
    PTE->PDDR |= 1 << led_pin_red;  

    PORTE->PCR[led_pin_green] = PORT_PCR_MUX(0b001);
    PTE->PSOR = 1 << led_pin_green;   
    PTE->PDDR |= 1 << led_pin_green;  

    // turn off the LEDS
    red_off();
    green_off();
}

/**
 * @brief Configures a GPIO pin for switch input with interrupt on the falling edge.
 * 
 * @param switch_pin The GPIO pin number to be set up for the switch input.
 * 
 * The function configures the switch pins on Port C as input with pull-up resistors
 * and interrupt on the falling edge.
 */
void init_switch(uint32_t switch_pin)
{
    PORTC->PCR[switch_pin] &= ~PORT_PCR_MUX_MASK;
    PORTC->PCR[switch_pin] |= PORT_PCR_MUX(0b001);
    PTC->PDDR &= ~(1U << switch_pin);
    PORTC->PCR[switch_pin] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[switch_pin] &= ~PORT_PCR_IRQC_MASK;
    PORTC->PCR[switch_pin] |= PORT_PCR_IRQC(0b1010); 
}

/**
 * @brief Initializes the GPIO pins for both switches using init_switch().
 */
void init_switch_pins(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    init_switch(switch_pin_1);
    init_switch(switch_pin_2);
}

/**
 * @brief Adjust LED brightness and car side directions (left, right) based on the accelerometer's tilt angle.
 * 
 * @param xAngle The measured tilt angle in degrees, which influences the car left/right directions and LED brightness.
 * @param xDuty_p Pointer to an integer that stores the calculated duty cycle percentage.
 *
 * The function calculates the duty cycle as a linear function of the angle between
 * defined upper and lower bounds.  
 */
void update_xDuty_cycle(int16_t xAngle, int16_t *xDuty_p)
{
    if (xAngle >= ANGLE_UPPER_BOUND) // If exceeds upper bound angle, set duty cycle to 100% and turn on the LED brightness to full.
    {
        *xDuty_p = 100;  
    }
    else if (xAngle <= -ANGLE_UPPER_BOUND) // If exceeds lower bound angle, set duty cycle to 100% and turn on the LED brightness to full.
    {
        *xDuty_p = 100; 
    }
    else if (xAngle > ANGLE_LOWER_BOUND && xAngle < ANGLE_UPPER_BOUND)
    {
        
        // Calculate duty cycle as a linear function of the angle between the bounds. Scalable LED brightness.
        *xDuty_p = (int16_t)(((xAngle - ANGLE_LOWER_BOUND) / (float)(ANGLE_UPPER_BOUND - ANGLE_LOWER_BOUND)) * 100);
    }
    else if (xAngle < -ANGLE_LOWER_BOUND && xAngle > -ANGLE_UPPER_BOUND)
    {
        // Calculate duty cycle as a linear function of the angle between the bounds. Scalable LED brightness.
        *xDuty_p = (int16_t)(((xAngle + ANGLE_LOWER_BOUND) / (float)(ANGLE_UPPER_BOUND - ANGLE_LOWER_BOUND)) * 100);
    }
    else
    {
        // Dead zone: If angle is within the bounds, set duty cycle to 0. Turn off the LED.
        *xDuty_p = 0;
    }

    // Based on tilt direction and its extent, print the direction to move the car.
    if (*xDuty_p != 0)
    {
        if (xAngle > 0)
        {
            PRINTF("a\n");  // Instruct car to move left.
        }
        else
        {
            PRINTF("d\n"); // Instruct car to move right.
        }
    }
}

/**
 * @brief Handles input from switches and performs acceleration or deacceleration. 
 * 
 * The function checks the interrupt status flag for each configured switch.
 * If the interrupt flag is set, indicating a button press, it clears the
 * interrupt and performs an action based on the specific switch:
 * - Switch 1 triggers an acceleration.
 * - Switch 2 triggers a deceleration.
 *
 */
void handle_switch_inputs(void)
{
    if (PORTC->PCR[switch_pin_1] & PORT_PCR_ISF_MASK)
    {
        clear_interrupt(switch_pin_1);
        PRINTF("w\n"); // Instruct car to accelerate.
    }

    if (PORTC->PCR[switch_pin_2] & PORT_PCR_ISF_MASK)
    {
        clear_interrupt(switch_pin_2);
        PRINTF("s\n"); // Instruct car to deaccelerate.
    }
}

/**
 * @brief Clears the interrupt status flag for a specified switch pin.
 *
 * @param sw_pin The pin number of the switch for which the interrupt flag
 *               needs to be cleared.
 */
void clear_interrupt(uint32_t sw_pin)
{
    PORTC->PCR[sw_pin] |= PORT_PCR_ISF_MASK;
}


/**
 * @brief Controls for turning on, off and toggling respective LEDs.
*/
void red_on(void)
{
    PTE->PCOR |= GPIO_PCOR_PTCO(1 << led_pin_red);
}
void red_off(void)
{
    PTE->PSOR |= GPIO_PSOR_PTSO(1 << led_pin_red);
}
void red_toggle(void)
{
    PTE->PTOR |= GPIO_PTOR_PTTO(1 << led_pin_red);
}
void green_on(void)
{
    PTE->PCOR |= GPIO_PCOR_PTCO(1 << led_pin_green);
}
void green_off(void)
{
    PTE->PSOR |= GPIO_PSOR_PTSO(1 << led_pin_green);
}
void green_toggle(void)
{
    PTE->PTOR |= GPIO_PTOR_PTTO(1 << led_pin_green);
}
