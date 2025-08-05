/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 * auth: ddhd
 */

#include <inttypes.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <nrfx_gpiote.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_gpiote.h>
#include <zephyr/irq.h>

#define SLEEP_TIME_MS 1
#define SIMULATED_INPUT_INTERVAL 1
#define SIM_IP_THREAD_PRIO 7
#define STACKSIZE 512

#define NRFX_ISR_PIN 20
// https://docs.nordicsemi.com/bundle/nrfx-apis-latest/page/group_configuration_of_c_m_s_i_s.html, gpiote1 for non secure
#define ISR_IRQn 47
#define ISR_PRIO 0
#define IRQ_FLAGS IRQ_ZERO_LATENCY

#define LOG_MODULE_NAME ngqt_main
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* Get the GPIO spec from devicetree */
static const struct gpio_dt_spec sense_input = GPIO_DT_SPEC_GET(DT_NODELABEL(sense_input), gpios);
static const struct gpio_dt_spec task_output = GPIO_DT_SPEC_GET(DT_NODELABEL(task_output), gpios);

// this is an OUTPUT from the SiP into itself
static const struct gpio_dt_spec sim_input = GPIO_DT_SPEC_GET(DT_NODELABEL(sim_input), gpios);

static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static struct gpio_callback sense_input_cb;

volatile uint8_t g_sens_cnt = 0;
uint32_t g_missed_event = 0;

k_tid_t main_thread_id;

struct sense_work
{
    struct k_work work;
    uint8_t cnt;
} sense_work_container;

void sense_work_fn(struct k_work *item)
{
    LOG_INF("sens_work_fn");
    /* clear output pin to see work latency from isr */
    gpio_pin_set_dt(&task_output, 0);
    if (g_sens_cnt > 0)
    {
        LOG_INF("sens_missed");
        g_missed_event++;
    }
    k_wakeup(main_thread_id);
}

/* Interrupt callback: set output high when input is high */
void sense_input_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    //!IMPORTANT: Do not use kernel APIs with ZLI, undefined behavior in this context 
    //LOG_INF("sens_ISR");
    if (nrf_gpiote_event_check(NRF_GPIOTE1, NRF_GPIOTE_EVENT_PORT)) {
        nrf_gpiote_event_clear(NRF_GPIOTE1, NRF_GPIOTE_EVENT_PORT);
    }
    nrf_gpio_pin_set(NRFX_ISR_PIN);
    g_sens_cnt++;
    

    /* Set the output pin high */
    // gpio_pin_set_dt(&task_output, 1);
    /* submit work item to offload irq */
    // k_work_submit(&sense_work_container.work);

    nrf_gpio_pin_clear(NRFX_ISR_PIN);
}

void simulate_input_thread(void)
{
    for (;;)
    {
        LOG_INF("sim_input");
        // pulse for 100us every input sleep interval
        gpio_pin_set_dt(&sim_input, 1);
        k_sleep(K_USEC(100));
        gpio_pin_set_dt(&sim_input, 0);
        k_sleep(K_MSEC(SIMULATED_INPUT_INTERVAL));
    }
}

#define SENSE_INPUT_PIN 3

int main(void)
{
    int ret = 0;

    main_thread_id = k_current_get();

    //NRF_Px_DIRSET = (1 << NRFX_ISR_PIN), followed with OUTSET/CLR.
    nrf_gpio_cfg_output(NRFX_ISR_PIN);

    /* Configure output pin */
    gpio_pin_configure_dt(&task_output, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&sim_input, GPIO_OUTPUT_INACTIVE);

    /* Configure input pin with interrupt on rising edge */
    // gpio_pin_configure_dt(&sense_input, GPIO_INPUT | GPIO_PULL_DOWN);
    // gpio_pin_interrupt_configure_dt(&sense_input, GPIO_INT_EDGE_RISING);

    /* Initialize and add callback */
    //gpio_init_callback(&sense_input_cb, sense_input_isr, BIT(sense_input.pin));
    // gpio_add_callback(sense_input.port, &sense_input_cb);
    
    nrf_gpio_cfg_input(SENSE_INPUT_PIN, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_sense_set(SENSE_INPUT_PIN, NRF_GPIO_PIN_SENSE_HIGH);
    nrf_gpiote_event_clear(NRF_GPIOTE1, NRF_GPIOTE_EVENT_PORT);
    nrf_gpiote_int_enable(NRF_GPIOTE1, GPIOTE_INTENSET_PORT_Msk);
    
    IRQ_DIRECT_CONNECT(ISR_IRQn, ISR_PRIO, sense_input_isr, NULL, IRQ_FLAGS);
    irq_enable(ISR_IRQn);

    /* set up LED */
    if (led.port && !gpio_is_ready_dt(&led))
    {
        LOG_INF("Error %d: LED device %s is not ready; ignoring it\n", ret, led.port->name);
        led.port = NULL;
    }
    if (led.port)
    {
        ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
        if (ret != 0)
        {
            LOG_INF("Error %d: failed to configure LED device %s pin %d\n", ret, led.port->name, led.pin);
            led.port = NULL;
        }
        else
        {
            LOG_INF("Set up LED at %s pin %d\n", led.port->name, led.pin);
        }
    }
    //set up kernel work q
    k_work_init(&sense_work_container.work, sense_work_fn);

    for (;;)
    {
        if (g_sens_cnt > 0)
        {
            g_sens_cnt--;
            LOG_INF("sens detected in main");
            gpio_pin_set_dt(&led, 0);
            k_sleep(K_MSEC(10));
            gpio_pin_set_dt(&led, 1);
        }
        LOG_INF("main sleeping until kernel work item wakes it");
        k_sleep(K_FOREVER);
    }

    return 0;
}

K_THREAD_DEFINE(sim_input_thread_id, STACKSIZE, simulate_input_thread, NULL, NULL, NULL, SIM_IP_THREAD_PRIO, 0, 0);
