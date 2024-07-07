#ifndef PC_METRICS_H
#define PC_METRICS_H

// A file containing various metrics made available from PC code.
// The performance impact from these should be fully toggleable,
// but toggling the RAM impact is optional.

#define PC_METRICS_ON 1
#define PC_METRICS_OFF 2

#define PC_METRICS_ENABLE PC_METRICS_OFF

#if PC_METRICS_ENABLE == PC_METRICS_ON
#define PC_METRIC_DO(cmd_) do { cmd_; } while (0)

#elif PC_METRICS_ENABLE == PC_METRICS_OFF
#define PC_METRIC_DO(cmd_) do {} while (0)

#else
prevent compile // Invalid PC_METRICS_ENABLE
#endif


extern int num_rsp_commands_run; // The number of RSP commands run since the last time this counter was cleared.

#endif
