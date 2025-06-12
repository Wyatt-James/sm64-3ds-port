#pragma once

/*
 * A simple polymorphic data type to resolve data based on an external variable.
 */

#include <stdbool.h>
#include <stddef.h>

#include "src/pc/pc_macros.h"

// Constructs an AutoAddr_Single
#define ADDR_SINGLE(tex_) (AutoAddr_Single) { \
        .data = tex_                          \
}

// Constructs an AutoAddr_Bool
#define ADDR_BOOL(ptr_, false_tex_, true_tex_) (AutoAddr_Bool) { \
    .selector = ptr_,                                            \
    .false_data = false_tex_,                                    \
    .true_data = true_tex_,                                      \
}

// Constructs an AutoAddr_Enum
#define ADDR_ENUM(ptr_, ...) (AutoAddr_Enum) {    \
    .selector = ptr_,                             \
    .count = VARARGS_COUNT(void*, __VA_ARGS__),   \
    .data = {__VA_ARGS__},                        \
}

// Constructs an AutoAddr of type SINGLE
#define AUTOADDR_SINGLE(tex_) (AutoAddr) { \
    .type = AUTOADDR_TYPE_SINGLE,          \
    .as_single = ADDR_SINGLE(tex_),        \
}

// Constructs an AutoAddr of type BOOL
#define AUTOADDR_BOOL(ptr_, false_tex_, true_tex_) (AutoAddr) { \
    .type = AUTOADDR_TYPE_BOOL,                                 \
    .as_bool = ADDR_BOOL(ptr_, false_tex_, true_tex_),          \
}

// Constructs an AutoAddr of type ENUM
#define AUTOADDR_ENUM(ptr_, ...) (AutoAddr) { \
    .type = AUTOADDR_TYPE_ENUM,               \
    .as_enum = ADDR_ENUM(ptr_, __VA_ARGS__),  \
}

typedef enum
{
    AUTOADDR_TYPE_SINGLE,
    AUTOADDR_TYPE_BOOL,
    AUTOADDR_TYPE_ENUM,
} AutoAddrType;

typedef struct
{
    void* data;
} AutoAddr_Single;

typedef struct
{
    bool* selector;
    void* false_data;
    void* true_data;
} AutoAddr_Bool;

typedef struct
{
    size_t* selector;
    size_t count;
    void* data[]; // Variable-length
} AutoAddr_Enum;

typedef struct
{
    AutoAddrType type;
    union
    {
        AutoAddr_Single as_single;
        AutoAddr_Bool as_bool;
        AutoAddr_Enum as_enum;
    };
} AutoAddr;

static inline void* autoaddr_resolve_single(AutoAddr_Single* tex)
{
    return tex->data;
}

static inline void* autoaddr_resolve_bool(AutoAddr_Bool* tex)
{
    if (tex->selector == NULL)
        return NULL;

    return tex->selector[0] ? tex->true_data : tex->false_data;
}

static inline void* autoaddr_resolve_enum(AutoAddr_Enum* tex)
{
    if (tex->count == 0)
        return NULL;

    return tex->data[ENUM_CLAMP(tex->selector[0], tex->count)];
}

static inline void* autoaddr_resolve(AutoAddr* tex)
{
    switch (tex->type)
    {
        case AUTOADDR_TYPE_SINGLE: return autoaddr_resolve_single(&tex->as_single);
        case AUTOADDR_TYPE_BOOL:   return autoaddr_resolve_bool(&tex->as_bool);
        case AUTOADDR_TYPE_ENUM:   return autoaddr_resolve_enum(&tex->as_enum);
        default:                  return NULL;
    }
}
