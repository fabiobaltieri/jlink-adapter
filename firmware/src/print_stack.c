#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/debug/thread_analyzer.h>

LOG_MODULE_REGISTER(print_stack, LOG_LEVEL_INF);

#define PRINT_STACK_INTERVAL_S 5

static void print_stack_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(print_stack_dwork, print_stack_handler);

static void print_stack_handler(struct k_work *work)
{
	thread_analyzer_print(0);

	k_work_schedule(&print_stack_dwork, K_SECONDS(PRINT_STACK_INTERVAL_S));
}

static int print_stack_init(void)
{
        k_work_schedule(&print_stack_dwork, K_SECONDS(PRINT_STACK_INTERVAL_S));

	return 0;
}

SYS_INIT(print_stack_init, APPLICATION, 91);
