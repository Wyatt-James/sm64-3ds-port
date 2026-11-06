#pragma once

#define N3DS_SECONDS_TO_NANOS(t) (t * 1000000000LL)   // Convert seconds to nanoseconds
#define N3DS_MILLIS_TO_NANOS(t)  (t * 1000000LL)      // Convert milliseconds to nanoseconds
#define N3DS_MICROS_TO_NANOS(t)  (t * 1000LL)         // Convert microseconds to nanoseconds
#define N3DS_NANOS_TO_SECONDS(t) (t / 1000000000LL)   // Convert nanoseconds to seconds
#define N3DS_NANOS_TO_MILLIS(t)  (t / 1000000LL)      // Convert nanoseconds to milliseconds
#define N3DS_NANOS_TO_MICROS(t)  (t / 1000LL)         // Convert nanoseconds to microseconds
#define N3DS_NANOS(t)            (t)                  // A duration in nanoseconds
