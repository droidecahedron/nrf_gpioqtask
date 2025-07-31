
# Overview

A simple  demo showcasing the use of GPIO input with interrupts.
The sample prints a message to the console each time a button is pressed.


# Requirements
`nRF9151DK`
`NCS v3.0.2`

The board hardware must have a push button connected via a GPIO pin. These are
called "User buttons" on many of Zephyr's :ref:`boards`.

The sample additionally supports an optional ``led0`` devicetree alias. This is
the same alias used by the :zephyr:code-sample:`blinky` sample. If this is provided, the LED
will be turned on when the button is pressed, and turned off off when it is
released.

# Devicetree details

```
/ {
    //we will simulate an output with p0.06 and feed that into p0.03
    sense_input: sense-input {
        compatible = "nordic,gpio-pins";
        gpios = <&gpio0 3 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>;
        status = "okay";
    };

    task_output: task-output {
        compatible = "nordic,gpio-pins";
        gpios = <&gpio0 2 (GPIO_ACTIVE_HIGH)>;
        status = "okay";
    };

    sim_input: sim_input {
        compatible = "nordic,gpio-pins";
        gpios = <&gpio0 6 (GPIO_ACTIVE_HIGH)>;
        status = "okay";
    };
};

&gpio0 {
    status = "okay";
};

// gpiote periph. https://docs.nordicsemi.com/bundle/ps_nrf9151/page/gpiote.html
&gpiote {
    status = "okay";
};
```


# Building and Running

- Connect `P0.06` to `P0.03`
- Probe `P0.06` or `P0.03`, `P0.00`, and `P0.02`
- Connect to `VCOM0` at the default `115.2,8,n,1` settings.

After startup, the program looks up a predefined GPIO device, and configures the
pin in input mode, enabling interrupt generation on rising edge. During each
iteration of the main loop, the state of GPIO line is monitored and printed to
the serial console. When the input button gets pressed, the interrupt handler
will print an information about this event along with its timestamp.


You will see that the `sim_input` pin will simulate an input, which triggers a work function and you can measure when the ISR happens, when the work function happens, and when the main thread detects a flag.

```
-- 1 messages dropped ---
[00:00:10.711,883] <inf> ngqt_main: sens_ISR
--- 2 messages dropped ---
[00:00:10.722,229] <inf> ngqt_main: sens_ISR
--- 2 messages dropped ---
[00:00:10.732,574] <inf> ngqt_main: sens_ISR
[00:00:10.732,604] <inf> ngqt_main: sens_work_fn
```

You will see dropped logs depending on the speed you are simulating inputs, and we are using deferred logging as well to keep it faster.

You can tighten the timing with `nrfx` instead of devicetree in ISRs, you can also measure how long from when the devicetree pin driving executes occurs in ISR versus the work queue item. You can also decide to have main be less sleepy and eat the flag more quickly to avoid "dropped" events.