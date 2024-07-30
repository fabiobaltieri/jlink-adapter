#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#define VOUT_NODE DT_NODELABEL(vout)
static const struct adc_dt_spec vout = ADC_DT_SPEC_GET(VOUT_NODE);

#define LED_WARN_IDX DT_NODE_CHILD_IDX(DT_NODELABEL(led_warn))
#define LED_1V8_IN DT_NODE_CHILD_IDX(DT_NODELABEL(led_1v8_in))
#define LED_3V3_IN DT_NODE_CHILD_IDX(DT_NODELABEL(led_3v3_in))
#define LED_1V8_OUT DT_NODE_CHILD_IDX(DT_NODELABEL(led_1v8_out))
#define LED_3V3_OUT DT_NODE_CHILD_IDX(DT_NODELABEL(led_3v3_out))

#define LED_GREEN_ON_LEVEL 20
#define LED_ON_LEVEL 60

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct device *led_dev = DEVICE_DT_GET(DT_NODELABEL(leds));

static uint32_t read_vout_mv(void)
{
	int16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};
	int32_t vout_mv;
	int err;

	adc_sequence_init_dt(&vout, &sequence);

	err = adc_read(vout.dev, &sequence);
	if (err < 0) {
		LOG_ERR("adc_read failed: %d", err);
		return 0;
	}

	vout_mv = buf;
	adc_raw_to_millivolts_dt(&vout, &vout_mv);

	vout_mv = vout_mv *
		DT_PROP(VOUT_NODE, full_ohms) /
		DT_PROP(VOUT_NODE, output_ohms);

	LOG_DBG("vout_mv=%d", vout_mv);

	return vout_mv;
}

static struct {
	bool en_1v8;
	bool en_3v3;
	bool locked;
	bool blink;
	int64_t lock_blink_stop;
} status;

static const struct device *reg_1v8 = DEVICE_DT_GET(DT_NODELABEL(ldo_1v8));
static const struct device *reg_3v3 = DEVICE_DT_GET(DT_NODELABEL(ldo_3v3));

static void blink_input_cb(struct input_event *evt, void *user_data)
{
        if (!evt->sync) {
                return;
        }

	if (evt->value != 1) {
		return;
	}

	switch (evt->code) {
	case INPUT_KEY_0:
		LOG_INF("button 3v3");
		if (status.en_3v3) {
			led_set_brightness(led_dev, LED_3V3_OUT, 0);
			regulator_disable(reg_3v3);
			status.en_3v3 = false;
			return;
		}

		if (status.locked) {
			status.lock_blink_stop = k_uptime_get() + 1000;
			LOG_WRN("locked");
			return;
		}

		led_set_brightness(led_dev, LED_3V3_OUT, LED_ON_LEVEL);
		regulator_enable(reg_3v3);
		status.en_3v3 = true;
		return;
	case INPUT_KEY_1:
		LOG_INF("button 1v8");
		if (status.en_1v8) {
			led_set_brightness(led_dev, LED_1V8_OUT, 0);
			regulator_disable(reg_1v8);
			status.en_1v8 = false;
			return;
		}

		if (status.locked) {
			status.lock_blink_stop = k_uptime_get() + 1000;
			LOG_WRN("locked");
			return;
		}

		led_set_brightness(led_dev, LED_1V8_OUT, LED_ON_LEVEL);
		regulator_enable(reg_1v8);
		status.en_1v8 = true;
		return;
	default:
		LOG_INF("unknown code: %d", evt->code);
		return;
	}
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(buttons)), blink_input_cb, NULL);

int main(void)
{
	uint32_t vout_mv;
	uint8_t on_level;

	while (1) {
		vout_mv = read_vout_mv();

		if (vout_mv < 100) {
			led_set_brightness(led_dev, LED_WARN_IDX, LED_ON_LEVEL);
			led_set_brightness(led_dev, LED_1V8_IN, 0);
			led_set_brightness(led_dev, LED_3V3_IN, 0);
			status.locked = false;
			goto out;
		}

		status.locked = true;

		if (k_uptime_get() > status.lock_blink_stop) {
			on_level = LED_GREEN_ON_LEVEL;
		} else {
			on_level = status.blink ? LED_GREEN_ON_LEVEL : 0;
		}

		if (IN_RANGE(vout_mv, 1700, 1900)) {
			led_set_brightness(led_dev, LED_WARN_IDX, 0);
			led_set_brightness(led_dev, LED_1V8_IN, on_level);
			led_set_brightness(led_dev, LED_3V3_IN, 0);
		} else if (IN_RANGE(vout_mv, 3200, 3400)) {
			led_set_brightness(led_dev, LED_WARN_IDX, 0);
			led_set_brightness(led_dev, LED_1V8_IN, 0);
			led_set_brightness(led_dev, LED_3V3_IN, on_level);
		} else {
			led_set_brightness(led_dev, LED_WARN_IDX, status.blink ? LED_ON_LEVEL : 0);
			led_set_brightness(led_dev, LED_1V8_IN, 0);
			led_set_brightness(led_dev, LED_3V3_IN, 0);
		}
out:
		status.blink = !status.blink;

		k_msleep(100);
	}

	return 0;
}
