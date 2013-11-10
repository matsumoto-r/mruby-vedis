/*
** mrb_vedis - vedis class for mruby using vedis
**
** Copyright (c) mod_mruby developers 2013-
**
** based on below vedis license.
*/
/*
 * Copyright (C) 2013 Symisc Systems, S.U.A.R.L [M.I.A.G Mrad Chems Eddine <chm@symisc.net>].
 * All rights reserved.
 *
 * vedistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. vedistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. vedistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. vedistributions in any form must be accompanied by information on
 *    how to obtain complete source code for the Vedis engine and any
 *    accompanying software that uses the Vedis engine software.
 *    The source code must either be included in the distribution
 *    or be available for no more than the cost of distribution plus
 *    a nominal fee, and must be freely vedistributable under reasonable
 *    conditions. For an executable file, complete source code means
 *    the source code for all modules it contains.It does not include
 *    source code for modules or files that typically accompany the major
 *    components of the operating system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO EVENT SHALL SYMISC SYSTEMS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include "vedis.h"
#include "mruby.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/class.h"
#include "mrb_vedis.h"

#define DONE mrb_gc_arena_restore(mrb, 0);

static void mrb_vedis_error(mrb_state *mrb, vedis *store, const char *msg)
{
    const char *err;
    int elen = 0;

    if (store) {
        vedis_config(store, VEDIS_CONFIG_ERR_LOG, &err, &elen);
        if (elen > 0) {
            vedis_lib_shutdown();
            mrb_raisef(mrb, E_RUNTIME_ERROR, "vedis error: %S", mrb_str_new_cstr(mrb, err));
        }
    } else {
        if (msg) {
            vedis_lib_shutdown();
            mrb_raisef(mrb, E_RUNTIME_ERROR, "vedis error: %S", mrb_str_new_cstr(mrb, msg));
        }
    }
    mrb_raise(mrb, E_RUNTIME_ERROR, "vedis unexpected error");
}

static void mrb_vedis_ctx_free(mrb_state *mrb, void *p)
{
    vedis_close(p);
}

static const struct mrb_data_type vedis_ctx_type = {
    "vedis_ctx", mrb_vedis_ctx_free,
};

static vedis *mrb_vedis_get_ctx(mrb_state *mrb,  mrb_value self)
{
    vedis *ctx;
    mrb_value ctx_obj;

    ctx_obj = mrb_iv_get(mrb, self, mrb_intern(mrb, "vedis_ctx"));
    Data_Get_Struct(mrb, ctx_obj, &vedis_ctx_type, ctx);
    if (!ctx)
        mrb_raise(mrb, E_RUNTIME_ERROR, "mrb_vedis_get_ctx get from mrb_iv_get vedis_ctx failed");

    return ctx;
}

mrb_value mrb_vedis_open(mrb_state *mrb, mrb_value self)
{
    int ret, argc;
    vedis *vstore;
    mrb_value db_file;

    argc = mrb_get_args(mrb, "|o", &db_file);

    ret = vedis_open(&vstore, argc == 0 ? ":mem:" : RSTRING_PTR(db_file));
    if (ret != VEDIS_OK) {
        mrb_vedis_error(mrb, 0, "Out of memory");
    }

    mrb_iv_set(mrb
        , self
        , mrb_intern(mrb, "vedis_ctx")
        , mrb_obj_value(Data_Wrap_Struct(mrb
            , mrb->object_class
            , &vedis_ctx_type
            , (void*) vstore)
        )
    );

    return self;
}

mrb_value mrb_vedis_set(mrb_state *mrb, mrb_value self)
{
    int ret;
    vedis *vstore;
    mrb_value key_obj, val_obj;
    const char *key = NULL;

    mrb_get_args(mrb, "oo", &key_obj, &val_obj);
    switch (mrb_type(key_obj)) {
        case MRB_TT_STRING:
            key = RSTRING_PTR(key_obj);
            break;
        case MRB_TT_SYMBOL:
            key = mrb_sym2name(mrb, mrb_obj_to_sym(mrb, key_obj));
            break;
        default:
            mrb_raise(mrb, E_RUNTIME_ERROR, "vedis key type is string or symbol");
    }
    vstore = mrb_vedis_get_ctx(mrb, self);
    val_obj = mrb_obj_as_string(mrb, val_obj);
    ret = vedis_kv_store(vstore, key, strlen(key), RSTRING_PTR(val_obj), RSTRING_LEN(val_obj));
    if (ret != VEDIS_OK) {
        mrb_vedis_error(mrb, vstore, 0);
    }

    return val_obj;
}

static mrb_value mrb_vedis_get(mrb_state *mrb, mrb_value self)
{
    int ret;
    vedis *vstore;
    mrb_value key_obj;
    vedis_value *result;
    const char *key = NULL;

    mrb_get_args(mrb, "o", &key_obj);
    switch (mrb_type(key_obj)) {
        case MRB_TT_STRING:
            key = RSTRING_PTR(key_obj);
            break;
        case MRB_TT_SYMBOL:
            key = mrb_sym2name(mrb, mrb_obj_to_sym(mrb, key_obj));
            break;
        default:
            mrb_raise(mrb, E_RUNTIME_ERROR, "vedis key type is string or symbol");
    }
    vstore = mrb_vedis_get_ctx(mrb, self);
    ret = vedis_exec_fmt(vstore, "GET %s", key);
    if (ret != VEDIS_OK) {
        return mrb_nil_value();
    }
    ret = vedis_exec_result(vstore, &result);
    if (ret != VEDIS_OK) {
        mrb_vedis_error(mrb, vstore, 0);
    } else {
        const char *val = vedis_value_to_string(result, 0);
        return mrb_str_new_cstr(mrb, val);
    }
    return mrb_nil_value();
}

static mrb_value mrb_vedis_del(mrb_state *mrb, mrb_value self)
{
    int ret;
    vedis *vstore;
    mrb_value key;

    mrb_get_args(mrb, "o", &key);
    vstore = mrb_vedis_get_ctx(mrb, self);
    ret = vedis_kv_delete(vstore, RSTRING_PTR(key), -1);
    if (ret != VEDIS_OK) {
        mrb_vedis_error(mrb, vstore, 0);
    }

    return key;
}

mrb_value mrb_vedis_append(mrb_state *mrb, mrb_value self)
{
    int ret;
    vedis *vstore;
    mrb_value key_obj, val_obj;
    const char *key = NULL;

    mrb_get_args(mrb, "oo", &key_obj, &val_obj);
    switch (mrb_type(key_obj)) {
        case MRB_TT_STRING:
            key = RSTRING_PTR(key_obj);
            break;
        case MRB_TT_SYMBOL:
            key = mrb_sym2name(mrb, mrb_obj_to_sym(mrb, key_obj));
            break;
        default:
            mrb_raise(mrb, E_RUNTIME_ERROR, "vedis key type is string or symbol");
    }
    vstore = mrb_vedis_get_ctx(mrb, self);
    val_obj = mrb_obj_as_string(mrb, val_obj);
    ret = vedis_kv_append(vstore, key, strlen(key), RSTRING_PTR(val_obj), RSTRING_LEN(val_obj));
    if (ret != VEDIS_OK) {
        mrb_vedis_error(mrb, vstore, 0);
    }

    return mrb_funcall(mrb, self, "get", 1, key_obj);
}

static mrb_value mrb_vedis_close(mrb_state *mrb, mrb_value self)
{
    int ret;
    vedis *vstore;

    vstore = mrb_vedis_get_ctx(mrb, self);
    ret = vedis_close(vstore);
    if (ret != VEDIS_OK) {
        mrb_vedis_error(mrb, vstore, 0);
    }

    return self;
}

void mrb_mruby_vedis_gem_init(mrb_state *mrb)
{
    struct RClass *vedis;

    vedis = mrb_define_class(mrb, "Vedis", mrb->object_class);

    mrb_define_method(mrb, vedis, "initialize", mrb_vedis_open, ARGS_OPT(1));
    mrb_define_method(mrb, vedis, "set", mrb_vedis_set, ARGS_REQ(2));
    mrb_define_method(mrb, vedis, "get", mrb_vedis_get, ARGS_REQ(1));
    mrb_define_method(mrb, vedis, "[]=", mrb_vedis_set, ARGS_REQ(2));
    mrb_define_method(mrb, vedis, "[]", mrb_vedis_get, ARGS_REQ(1));
    mrb_define_method(mrb, vedis, "del", mrb_vedis_del, ARGS_REQ(1));
    mrb_define_method(mrb, vedis, "append", mrb_vedis_append, ARGS_REQ(2));
    mrb_define_method(mrb, vedis, "close", mrb_vedis_close, ARGS_NONE());
    DONE;
}

void mrb_mruby_vedis_gem_final(mrb_state *mrb)
{
}

//#endif
