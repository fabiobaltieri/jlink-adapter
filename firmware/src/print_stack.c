#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(print_stack, LOG_LEVEL_INF);

#define PRINT_STACK_INTERVAL_S 5

static void print_stack_cb(const struct k_thread *cthread, void *user_data)
{
	        struct k_thread *thread = (struct k_thread *)cthread;
		size_t unused;
		size_t size = thread->stack_info.size;
		int ret;

		ret = k_thread_stack_space_get(thread, &unused);
		if (ret < 0) {
			LOG_ERR("thread_stack_space_get failed: %d", ret);
			return;
		}

		LOG_INF("%s: used=%d/%d",
		       k_thread_name_get(thread), size - unused, size);
}

static void print_stack_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(print_stack_dwork, print_stack_handler);

static void print_stack_handler(struct k_work *work)
{
	k_thread_foreach(print_stack_cb, NULL);

	k_work_schedule(&print_stack_dwork, K_SECONDS(PRINT_STACK_INTERVAL_S));
}

static int print_stack_init(void)
{
        k_work_schedule(&print_stack_dwork, K_SECONDS(PRINT_STACK_INTERVAL_S));

	return 0;
}

SYS_INIT(print_stack_init, APPLICATION, 91);
