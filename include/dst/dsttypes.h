/*
* Copyright (c) 2017 Calvin Rose
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#ifndef DST_TYPES_H_defined
#define DST_TYPES_H_defined

#include <stdint.h>
#include "dstconfig.h"

#ifdef DST_NANBOX
typedef union DstValue DstValue;
#else
typedef struct DstValue DstValue;
#endif

/* All of the dst types */
typedef struct DstFunction DstFunction;
typedef struct DstArray DstArray;
typedef struct DstBuffer DstBuffer;
typedef struct DstTable DstTable;
typedef struct DstFiber DstFiber;

/* Other structs */
typedef struct DstReg DstReg;
typedef struct DstUserdataHeader DstUserdataHeader;
typedef struct DstFuncDef DstFuncDef;
typedef struct DstFuncEnv DstFuncEnv;
typedef struct DstStackFrame DstStackFrame;
typedef struct DstUserType DstUserType;
typedef int (*DstCFunction)(int32_t argn, DstValue *argv, DstValue *ret);

typedef enum DstAssembleStatus DstAssembleStatus;
typedef struct DstAssembleResult DstAssembleResult;
typedef struct DstAssembleOptions DstAssembleOptions;
typedef enum DstCompileStatus DstCompileStatus;
typedef struct DstCompileOptions DstCompileOptions;
typedef struct DstCompileResult DstCompileResult;
typedef struct DstParseResult DstParseResult;
typedef enum DstParseStatus DstParseStatus;

/* Basic types for all Dst Values */
typedef enum DstType {
    DST_NIL,
    DST_FALSE,
    DST_TRUE,
    DST_FIBER,
    DST_INTEGER,
    DST_REAL,
    DST_STRING,
    DST_SYMBOL,
    DST_ARRAY,
    DST_TUPLE,
    DST_TABLE,
    DST_STRUCT,
    DST_BUFFER,
    DST_FUNCTION,
    DST_CFUNCTION,
    DST_USERDATA
} DstType;

/* We provide two possible implemenations of DstValues. The preferred
 * nanboxing approach, and the standard C version. Code in the rest of the
 * application must interact through exposed interface. */

/* Required interface for DstValue */
/* wrap and unwrap for all types */
/* Get type quickly */
/* Check against type quickly */
/* Small footprint */
/* 32 bit integer support */

/* dst_type(x)
 * dst_checktype(x, t)
 * dst_wrap_##TYPE(x)
 * dst_unwrap_##TYPE(x)
 * dst_truthy(x)
 * dst_memclear(p, n) - clear memory for hash tables to nils
 * dst_u64(x) - get 64 bits of payload for hashing
 */

#ifdef DST_NANBOX

#include <math.h>

union DstValue {
    uint64_t u64;
    int64_t i64;
    void *pointer;
    const void *cpointer;
    double real;
};

/* This representation uses 48 bit pointers. The trade off vs. the LuaJIT style
 * 47 bit payload representaion is that the type bits are no long contiguous. Type
 * checking can still be fast, but typewise polymorphism takes a bit longer. However, 
 * hopefully we can avoid some annoying problems that occur when trying to use 47 bit pointers
 * in a 48 bit address space (Linux on ARM) */

/*                    |.......Tag.......|.......................Payload..................| */
/* Non-double:        t|11111111111|1ttt|xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */
/* Types of NIL, TRUE, and FALSE must have payload set to all 1s. */

/* Double (no NaNs):   x xxxxxxxxxxx xxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

/* A simple scheme for nan boxed values */
/* normal doubles, denormalized doubles, and infinities are doubles */
/* Quiet nan is nil. Sign bit should be 0. */

#define DST_NANBOX_TYPEBITS    0x0007000000000000lu
#define DST_NANBOX_TAGBITS     0xFFFF000000000000lu
#define DST_NANBOX_PAYLOADBITS 0x0000FFFFFFFFFFFFlu
#ifdef DST_64
#define DST_NANBOX_POINTERBITS 0x0000FFFFFFFFFFFFlu
#else
#define DST_NANBOX_POINTERBITS 0x00000000FFFFFFFFlu
#endif

#define dst_u64(x) ((x).u64)
#define dst_nanbox_lowtag(type) \
    ((((uint64_t)(type) & 0x8) << 12) | 0x7FF8 | (type))
#define dst_nanbox_tag(type) \
    (dst_nanbox_lowtag(type) << 48)

#define dst_nanbox_checkauxtype(x, type) \
    (((x).u64 & DST_NANBOX_TAGBITS) == dst_nanbox_tag((type)))

/* Check if number is nan or if number is real double */
#define dst_nanbox_isreal(x) \
    (!isnan((x).real) || dst_nanbox_checkauxtype((x), DST_REAL))

#define dst_type(x) \
    (isnan((x).real) \
        ? (((x).u64 & DST_NANBOX_TYPEBITS) >> 48) | (((x).u64 >> 60) & 0x8) \
        : DST_REAL)

#define dst_checktype(x, t) \
    (((t) == DST_REAL) \
        ? dst_nanbox_isreal(x) \
        : dst_nanbox_checkauxtype((x), (t)))

void *dst_nanbox_to_pointer(DstValue x);
void dst_nanbox_memempty(DstValue *mem, int32_t count);
void *dst_nanbox_memalloc_empty(int32_t count);
DstValue dst_nanbox_from_pointer(void *p, uint64_t tagmask);
DstValue dst_nanbox_from_cpointer(const void *p, uint64_t tagmask);
DstValue dst_nanbox_from_double(double d);
DstValue dst_nanbox_from_bits(uint64_t bits);

#define dst_memempty(mem, len) dst_nanbox_memempty((mem), (len))
#define dst_memalloc_empty(count) dst_nanbox_memalloc_empty(count)

/* Todo - check for single mask operation */
#define dst_truthy(x) \
    (!(dst_checktype((x), DST_NIL) || dst_checktype((x), DST_FALSE)))

#define dst_nanbox_from_payload(t, p) \
    dst_nanbox_from_bits(dst_nanbox_tag(t) | (p))

#define dst_nanbox_wrap_(p, t) \
    dst_nanbox_from_pointer((p), dst_nanbox_tag(t) | 0x7FF8000000000000lu)

#define dst_nanbox_wrap_c(p, t) \
    dst_nanbox_from_cpointer((p), dst_nanbox_tag(t) | 0x7FF8000000000000lu)

/* Wrap the simple types */
#define dst_wrap_nil() dst_nanbox_from_payload(DST_NIL, 1)
#define dst_wrap_true() dst_nanbox_from_payload(DST_TRUE, 1)
#define dst_wrap_false() dst_nanbox_from_payload(DST_FALSE, 1)
#define dst_wrap_boolean(b) dst_nanbox_from_payload((b) ? DST_TRUE : DST_FALSE, 1)
#define dst_wrap_integer(i) dst_nanbox_from_payload(DST_INTEGER, (uint32_t)(i))
#define dst_wrap_real(r) dst_nanbox_from_double(r)

/* Unwrap the simple types */
#define dst_unwrap_boolean(x) \
    (((x).u64 >> 48) == dst_nanbox_lowtag(DST_TRUE))
#define dst_unwrap_integer(x) \
    ((int32_t)((x).u64 & 0xFFFFFFFFlu))
#define dst_unwrap_real(x) ((x).real)

/* Wrap the pointer types */
#define dst_wrap_struct(s) dst_nanbox_wrap_c((s), DST_STRUCT)
#define dst_wrap_tuple(s) dst_nanbox_wrap_c((s), DST_TUPLE)
#define dst_wrap_fiber(s) dst_nanbox_wrap_((s), DST_FIBER)
#define dst_wrap_array(s) dst_nanbox_wrap_((s), DST_ARRAY)
#define dst_wrap_table(s) dst_nanbox_wrap_((s), DST_TABLE)
#define dst_wrap_buffer(s) dst_nanbox_wrap_((s), DST_BUFFER)
#define dst_wrap_string(s) dst_nanbox_wrap_c((s), DST_STRING)
#define dst_wrap_symbol(s) dst_nanbox_wrap_c((s), DST_SYMBOL)
#define dst_wrap_userdata(s) dst_nanbox_wrap_((s), DST_USERDATA)
#define dst_wrap_pointer(s) dst_nanbox_wrap_((s), DST_USERDATA)
#define dst_wrap_function(s) dst_nanbox_wrap_((s), DST_FUNCTION)
#define dst_wrap_cfunction(s) dst_nanbox_wrap_((s), DST_CFUNCTION)

/* Unwrap the pointer types */
#define dst_unwrap_struct(x) ((const DstValue *)dst_nanbox_to_pointer(x))
#define dst_unwrap_tuple(x) ((const DstValue *)dst_nanbox_to_pointer(x))
#define dst_unwrap_fiber(x) ((DstFiber *)dst_nanbox_to_pointer(x))
#define dst_unwrap_array(x) ((DstArray *)dst_nanbox_to_pointer(x))
#define dst_unwrap_table(x) ((DstTable *)dst_nanbox_to_pointer(x))
#define dst_unwrap_buffer(x) ((DstBuffer *)dst_nanbox_to_pointer(x))
#define dst_unwrap_string(x) ((const uint8_t *)dst_nanbox_to_pointer(x))
#define dst_unwrap_symbol(x) ((const uint8_t *)dst_nanbox_to_pointer(x))
#define dst_unwrap_userdata(x) (dst_nanbox_to_pointer(x))
#define dst_unwrap_pointer(x) (dst_nanbox_to_pointer(x))
#define dst_unwrap_function(x) ((DstFunction *)dst_nanbox_to_pointer(x))
#define dst_unwrap_cfunction(x) ((DstCFunction)dst_nanbox_to_pointer(x))

/* End of [#ifdef DST_NANBOX] */
#else

/* A general dst value type */
struct DstValue {
    union {
        uint64_t u64;
        double real;
        int32_t integer;
        void *pointer;
        const void *cpointer;
    } as;
    DstType type;
};

#define dst_u64(x) ((x).as.u64)
#define dst_memempty(mem, count) memset((mem), 0, sizeof(DstValue) * (count))
#define dst_memalloc_empty(count) calloc((count), sizeof(DstValue))
#define dst_type(x) ((x).type)
#define dst_checktype(x, t) ((x).type == (t))
#define dst_truthy(x) \
    ((x).type != DST_NIL && (x).type != DST_FALSE)

#define dst_unwrap_struct(x) ((const DstValue *)(x).as.pointer)
#define dst_unwrap_tuple(x) ((const DstValue *)(x).as.pointer)
#define dst_unwrap_fiber(x) ((DstFiber *)(x).as.pointer)
#define dst_unwrap_array(x) ((DstArray *)(x).as.pointer)
#define dst_unwrap_table(x) ((DstTable *)(x).as.pointer)
#define dst_unwrap_buffer(x) ((DstBuffer *)(x).as.pointer)
#define dst_unwrap_string(x) ((const uint8_t *)(x).as.pointer)
#define dst_unwrap_symbol(x) ((const uint8_t *)(x).as.pointer)
#define dst_unwrap_userdata(x) ((x).as.pointer)
#define dst_unwrap_pointer(x) ((x).as.pointer)
#define dst_unwrap_function(x) ((DstFunction *)(x).as.pointer)
#define dst_unwrap_cfunction(x) ((DstCFunction)(x).as.pointer)
#define dst_unwrap_boolean(x) ((x).type == DST_TRUE)
#define dst_unwrap_integer(x) ((x).as.integer)
#define dst_unwrap_real(x) ((x).as.real)

DstValue dst_wrap_nil();
DstValue dst_wrap_real(double x);
DstValue dst_wrap_integer(int32_t x);
DstValue dst_wrap_true();
DstValue dst_wrap_false();
DstValue dst_wrap_boolean(int x);
DstValue dst_wrap_string(const uint8_t *x);
DstValue dst_wrap_symbol(const uint8_t *x);
DstValue dst_wrap_array(DstArray *x);
DstValue dst_wrap_tuple(const DstValue *x);
DstValue dst_wrap_struct(const DstValue *x);
DstValue dst_wrap_fiber(DstFiber *x);
DstValue dst_wrap_buffer(DstBuffer *x);
DstValue dst_wrap_function(DstFunction *x);
DstValue dst_wrap_cfunction(DstCFunction x);
DstValue dst_wrap_table(DstTable *x);
DstValue dst_wrap_userdata(void *x);
DstValue dst_wrap_pointer(void *x);

/* End of tagged union implementation */
#endif

/* Used for creating libraries of cfunctions. */
struct DstReg {
    const char *name;
    DstCFunction function;
};

/* A lightweight green thread in dst. Does not correspond to
 * operating system threads. */
struct DstFiber {
    DstValue *data;
    DstFiber *parent;
    int32_t frame; /* Index of the stack frame */
    int32_t frametop; /* Index of top of stack frame */
    int32_t stacktop; /* Top of stack. Where values are pushed and popped from. */
    int32_t capacity;
    enum {
        DST_FIBER_PENDING = 0,
        DST_FIBER_ALIVE,
        DST_FIBER_DEAD,
        DST_FIBER_ERROR
    } status;
};

/* A stack frame on the fiber. Is stored along with the stack values. */
struct DstStackFrame {
    DstFunction *func;
    uint32_t *pc;
    int32_t prevframe;
};

/* Number of DstValues a frame takes up in the stack */
#define DST_FRAME_SIZE ((sizeof(DstStackFrame) + sizeof(DstValue) - 1)/ sizeof(DstValue))

/* A dynamic array type. */
struct DstArray {
    DstValue *data;
    int32_t count;
    int32_t capacity;
};

/* A bytebuffer type. Used as a mutable string or string builder. */
struct DstBuffer {
    uint8_t *data;
    int32_t count;
    int32_t capacity;
};

/* A mutable associative data type. Backed by a hashtable. */
struct DstTable {
    DstValue *data;
    int32_t count;
    int32_t capacity;
    int32_t deleted;
};

/* Some function defintion flags */
#define DST_FUNCDEF_FLAG_VARARG 1
#define DST_FUNCDEF_FLAG_NEEDSENV 4

/* A function definition. Contains information needed to instantiate closures. */
struct DstFuncDef {
    int32_t *environments; /* Which environments to capture from parent. */
    DstValue *constants; /* Contains strings, FuncDefs, etc. */
    uint32_t *bytecode;

    /* Various debug information */
    int32_t *sourcemap;
    const uint8_t *source;
    const uint8_t *sourcepath;

    uint32_t flags;
    int32_t slotcount; /* The amount of stack space required for the function */
    int32_t arity; /* Not including varargs */
    int32_t constants_length;
    int32_t bytecode_length;
    int32_t environments_length; 
};

/* A fuction environment */
struct DstFuncEnv {
    union {
        DstFiber *fiber;
        DstValue *values;
    } as;
    int32_t length; /* Size of environment */
    int32_t offset; /* Stack offset when values still on stack. If offset is <= 0, then
        environment is no longer on the stack. */
};

/* A function */
struct DstFunction {
    DstFuncDef *def;
    /* Consider allocating envs with entire function struct */
    DstFuncEnv **envs;
};

/* Defines a type for userdata */
struct DstUserType {
    const char *name;
    int (*serialize)(void *data, size_t len);
    int (*deserialize)();
    void (*finalize)(void *data, size_t len);
};

/* Contains information about userdata */
struct DstUserdataHeader {
    const DstUserType *type;
    size_t size;
};

/* Assemble structs */
enum DstAssembleStatus {
    DST_ASSEMBLE_OK,
    DST_ASSEMBLE_ERROR
};

struct DstAssembleOptions {
    const DstValue *sourcemap;
    DstValue source;
    uint32_t flags;
};

struct DstAssembleResult {
    DstFuncDef *funcdef;
    const uint8_t *error;
    int32_t error_start;
    int32_t error_end;
    DstAssembleStatus status;
};

/* Compile structs */
enum DstCompileStatus {
    DST_COMPILE_OK,
    DST_COMPILE_ERROR
};

struct DstCompileResult {
    DstCompileStatus status;
    DstFuncDef *funcdef;
    const uint8_t *error;
    int32_t error_start;
    int32_t error_end;
};

struct DstCompileOptions {
    uint32_t flags;
    const DstValue *sourcemap;
    DstValue source;
    DstValue env;
};

/* Parse structs */
enum DstParseStatus {
    DST_PARSE_OK,
    DST_PARSE_ERROR,
    DST_PARSE_UNEXPECTED_EOS,
    DST_PARSE_NODATA
};

struct DstParseResult {
    DstValue value;
    const uint8_t *error;
    const DstValue *map;
    int32_t bytes_read;
    DstParseStatus status;
};

#endif /* DST_TYPES_H_defined */
