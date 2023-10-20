/*******************************************************************************
 * SVG Clock renderer - Enterprise Edition
 *******************************************************************************
 * Copyright (c) 2021 András Bodor <bodand@pm.me>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/
/*
 * A kód teljes mértékben saját fejlesztésű. Egyedül az adott Mozilla
 * SVG Tutorial került felhasználásra, illetve perror(3), printf(3), és calloc(3)
 * manpagek.
 *
 * Tudomásom szerint C11 kompatibilis, bár a C szabványt nem tudom olyan biztosan.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
 
void*
xmalloc(size_t sz) {
    void* mem = malloc(sz);
    if (!mem) {
        perror("malloc");
        exit(-1);
    }
    return mem;
}
 
void*
xcalloc(size_t cnt, size_t obj_sz) {
    void* mem = calloc(cnt, obj_sz);
    if (!mem) {
        perror("calloc");
        exit(-1);
    }
    return mem;
}
 
void
xfprintf(FILE* f, const char* restrict fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int printed = vfprintf(f, fmt, args);
    if (printed < 0) {
        perror("vfprintf");
        exit(-1);
    }
    va_end(args);
}
 
struct SVG_string {
    char* _bytes;
    size_t _size;
    size_t _length;
};
#define SVG_FMT(s) ((s)._length), ((s)._bytes)
 
struct SVG_string*
SVG_string_new(size_t size) {
    struct SVG_string* str = xmalloc(sizeof(struct SVG_string));
    *str = (struct SVG_string) {
           ._bytes = size ? xmalloc(size) : 0,
           ._size = size,
           ._length = 0,
    };
    return str;
}
 
void
SVG_string_destroy(struct SVG_string** str) {
    assert(str);
    assert(*str);
    free((*str)->_bytes);
    free(*str);
    *str = 0;
}
 
void
SVG_string_assign(struct SVG_string* str, const char* cstr) {
    assert(str);
    assert(cstr);
    size_t cstr_len = strlen(cstr);
    if (cstr_len > str->_size) {
        str->_length = str->_size = cstr_len;
        free(str->_bytes);
        str->_bytes = xmalloc(str->_size);
    } else {
        str->_length = cstr_len;
    }
    memcpy(str->_bytes, cstr, cstr_len);
}
 
void
SVG_string_copy(struct SVG_string* new_str,
                struct SVG_string* old_str) {
    size_t old_len = old_str->_length;
    if (old_len > new_str->_size) {
        new_str->_length = new_str->_size = old_len;
        free(new_str->_bytes);
        new_str->_bytes = xmalloc(new_str->_size);
    } else {
        new_str->_length = old_len;
    }
    memcpy(new_str->_bytes, old_str->_bytes, old_len);
}
 
enum SVG_value_type {
    SVG_NULL,
    SVG_INT,
    SVG_FLOAT,
    SVG_STRING,
    SVG_COORD,
};
struct SVG_param_list {
    struct SVG_string* _name;
    enum SVG_value_type _type;
    void* _value;
    struct SVG_param_list* _next;
};
typedef struct SVG_param_list* SVG_param;
 
struct SVG_param_list*
SVG_param_list_new(void) {
    struct SVG_param_list* pl = xmalloc(sizeof(struct SVG_param_list));
    pl->_name = 0;
    pl->_type = SVG_NULL;
    pl->_value = 0;
    pl->_next = 0;
    return pl;
}
 
struct SVG_param_list*
SVG_param_list_add(struct SVG_param_list* pl,
                   const char* const str,
                   const enum SVG_value_type value_type,
                   void* const value,
                   size_t value_sz) {
    assert(pl);
    assert(str);
    assert(value);
 
    SVG_param insert_into = pl;
    while (insert_into->_type != SVG_NULL) insert_into = insert_into->_next;
 
    insert_into->_name = SVG_string_new(0);
    SVG_string_assign(insert_into->_name, str);
    insert_into->_type = value_type;
 
    if (insert_into->_type == SVG_STRING) {
        struct SVG_string* val = value;
        insert_into->_value = SVG_string_new(val->_length);
        SVG_string_copy(insert_into->_value, val);
    } else {
        insert_into->_value = xmalloc(value_sz);
        memcpy(insert_into->_value, value, value_sz);
    }
 
    insert_into->_next = SVG_param_list_new();
 
    return insert_into;
}
 
void
SVG_param_list_destroy(struct SVG_param_list* params) {
    assert(params);
    SVG_param it = params;
    while (it->_type != SVG_NULL) {
        SVG_string_destroy(&it->_name);
        if (it->_type == SVG_STRING) {
            SVG_string_destroy(((struct SVG_string**) &it->_value));
            free(it->_value);
        } else {
            free(it->_value);
        }
        SVG_param to_free = it;
        it = it->_next;
        free(to_free);
    }
    free(it);
}
 
struct SVG_param_iteration {
    struct SVG_param_list* _it;
};
 
struct SVG_param_iteration
SVG_param_it_begin(struct SVG_param_list* pl) {
    assert(pl && "instead of supplying null use SVG_param_it_end");
 
    return (struct SVG_param_iteration) {
           ._it = pl
    };
}
 
void
SVG_param_it_next(struct SVG_param_iteration* it) {
    assert(it);
    assert(it->_it);
 
    it->_it = it->_it->_next;
}
 
struct SVG_param_iteration
SVG_param_it_end(void) {
    return (struct SVG_param_iteration) {
           ._it = 0
    };
}
 
int
SVG_param_it_eq(struct SVG_param_iteration* a,
                struct SVG_param_iteration* b) {
    assert(a);
    assert(b);
 
    return a->_it == b->_it;
}
 
SVG_param
SVG_param_it_deref(struct SVG_param_iteration* it) {
    assert(it->_it != 0);
 
    return it->_it;
}
 
 
void
SVG_param_print(SVG_param param, FILE* outp) {
    assert(param);
    assert(outp);
 
    switch (param->_type) {
        case SVG_NULL:
            /*do nothing*/
            return;
        case SVG_INT:
            xfprintf(outp, "%.*s=\"%d\"",
                     SVG_FMT(*param->_name),
                     *(int*) param->_value);
            break;
        case SVG_FLOAT:
            xfprintf(outp, "%.*s=\"%.1f\"",
                     SVG_FMT(*param->_name),
                     (double) *(float*) param->_value);
            break;
        case SVG_STRING:
            xfprintf(outp, "%.*s=\"%.*s\"",
                     SVG_FMT(*param->_name),
                     SVG_FMT((*(struct SVG_string*) param->_value)));
            break;
        case SVG_COORD:
            xfprintf(outp, "%.*s=\"%.4f\"",
                     SVG_FMT(*param->_name),
                     (double) *(float*) param->_value);
            break;
    }
}
 
void
SVG_param_list_print(struct SVG_param_list* params, FILE* outp) {
    for (struct SVG_param_iteration
                it = SVG_param_it_begin(params),
                end = SVG_param_it_end();
         !SVG_param_it_eq(&it, &end);
         SVG_param_it_next(&it)) {
        SVG_param param = SVG_param_it_deref(&it);
        SVG_param_print(param, outp);
        putc(' ', outp);
    }
}
 
struct SVG_shape;
 
typedef int (* SVG_proc_draw_start)(struct SVG_shape* this, FILE* outp);
typedef int (* SVG_proc_draw_finish)(struct SVG_shape* this, FILE* outp);
typedef void (* SVG_proc_draw_destroy)(struct SVG_shape* this);
 
struct SVG_shape {
    SVG_proc_draw_start _start;
    SVG_proc_draw_finish _finish;
    SVG_proc_draw_destroy _destroy; // should be in a vtable, but no
    struct SVG_param_list* _params;
    struct SVG_shape** _children;
    size_t _chld_sz;
    size_t _chld_len;
    void* _userdata;
};
 
#define SVG_CAT(x, y) SVG_CAT_I(x, y)
#define SVG_CAT_I(x, y) x##y
 
// member call for pseudo-oo
#define MBR_CALL(obj, fn) (obj)->SVG_CAT(_, fn)
 
struct SVG_shape*
SVG_shape_add_child(struct SVG_shape* this, struct SVG_shape* chld) {
    assert(this);
    if (this->_chld_sz == 0
        || this->_chld_len > this->_chld_sz - 1) {
        this->_chld_sz = (size_t) (ceill(this->_chld_sz * 1.5l) + 1);
 
        struct SVG_shape** new_children = xcalloc(this->_chld_sz, sizeof(chld));
        memcpy(new_children, this->_children, this->_chld_len * sizeof(this->_children[0]));
        struct SVG_shape** old_children = this->_children;
        this->_children = new_children;
        free(old_children);
    }
    this->_children[this->_chld_len++] = chld;
    return this->_children[this->_chld_len - 1];
}
 
void
SVG_print(struct SVG_shape* shp, FILE* outp) {
    if (MBR_CALL(shp, start)(shp, outp)) exit(-1);
    if (shp->_chld_len) {
        for (size_t i = 0; i < shp->_chld_len; ++i) {
            SVG_print(shp->_children[i], outp);
        }
    }
    if (MBR_CALL(shp, finish)(shp, outp)) exit(-1);
}
 
void
SVGSHP_universal_destroy(struct SVG_shape* this) {
    if (this->_chld_len) {
        for (size_t i = 0; i < this->_chld_len; ++i) {
            MBR_CALL(this->_children[i], destroy)(this->_children[i]);
        }
    }
    free(this->_children);
    SVG_param_list_destroy(this->_params);
    free(this);
}
 
int
SVGSHP_nl_finish(struct SVG_shape* this, FILE* outp) {
    (void) this;
    xfprintf(outp, "\n");
    return 0;
}
 
///////////////////////////////////// ROOT /////////////////////////////////////
int
SVGSHP_root_start(struct SVG_shape* this, FILE* outp) {
    xfprintf(outp, "<svg ");
    SVG_param_list_print(this->_params, outp);
    xfprintf(outp, ">\n");
    return 0;
}
 
int
SVGSHP_root_finish(struct SVG_shape* this, FILE* outp) {
    (void) this;
    xfprintf(outp, "</svg>");
    return 0;
}
 
struct SVG_shape*
SVGSHP_root_new(int width, int height) {
#define SVG_XMLNS "http://www.w3.org/2000/svg"
    struct SVG_string* xmlns = SVG_string_new(sizeof(SVG_XMLNS) - 1);
    SVG_string_assign(xmlns, SVG_XMLNS);
 
    struct SVG_shape* shp = xmalloc(sizeof(struct SVG_shape));
    *shp = (struct SVG_shape) {
           ._start = &SVGSHP_root_start,
           ._finish = &SVGSHP_root_finish,
           ._destroy = &SVGSHP_universal_destroy,
           ._params = SVG_param_list_new(),
           ._children = 0,
           ._chld_sz = 0,
           ._chld_len = 0,
           ._userdata = 0,
    };
    SVG_param_list_add(shp->_params, "width", SVG_INT, &width, sizeof width);
    SVG_param_list_add(shp->_params, "height", SVG_INT, &height, sizeof height);
    SVG_param_list_add(shp->_params, "xmlns", SVG_STRING, xmlns, sizeof xmlns);
    SVG_param_list_add(shp->_params, "version", SVG_FLOAT, &(float) {1.1f}, sizeof(float));
 
    SVG_string_destroy(&xmlns);
    return shp;
}
 
/////////////////////////////////// CONTENT ////////////////////////////////////
int
SVGSHP_content_start(struct SVG_shape* this, FILE* outp) {
    xfprintf(outp, "%.*s\n", SVG_FMT(*(struct SVG_string*) this->_userdata));
    return 0;
}
 
int
SVGSHP_content_finish(struct SVG_shape* this, FILE* outp) {
    (void) this;
    (void) outp;
    return 0;
}
 
void
SVGSHP_content_destroy(struct SVG_shape* this) {
    if (this->_chld_len) {
        for (size_t i = 0; i < this->_chld_len; ++i) {
            MBR_CALL(this->_children[i], destroy)(this->_children[i]);
        }
    }
    free(this->_children);
    SVG_string_destroy((struct SVG_string**) &this->_userdata);
    free(this);
}
 
struct SVG_shape*
SVGSHP_content_new(const char* content) {
    struct SVG_shape* shp = xmalloc(sizeof(struct SVG_shape));
    *shp = (struct SVG_shape) {
           ._start = &SVGSHP_content_start,
           ._finish = &SVGSHP_content_finish,
           ._destroy = &SVGSHP_content_destroy,
           ._params = 0,
           ._children = 0,
           ._chld_sz = 0,
           ._chld_len = 0,
           ._userdata = SVG_string_new(0),
    };
 
    SVG_string_assign(shp->_userdata, content);
    return shp;
}
 
///////////////////////////////////// TEXT /////////////////////////////////////
int
SVGSHP_text_start(struct SVG_shape* this, FILE* outp) {
    xfprintf(outp, "<text ");
    SVG_param_list_print(this->_params, outp);
    xfprintf(outp, ">\n");
    return 0;
}
 
int
SVGSHP_text_finish(struct SVG_shape* this, FILE* outp) {
    (void) this;
    xfprintf(outp, "</text>\n");
    return 0;
}
 
struct SVG_shape*
SVGSHP_text_new(int x, int y,
                int font_size,
                const char* content,
                struct SVG_string* fill) {
    struct SVG_shape* shp = xmalloc(sizeof(struct SVG_shape));
    *shp = (struct SVG_shape) {
           ._start = &SVGSHP_text_start,
           ._finish = &SVGSHP_text_finish,
           ._destroy = &SVGSHP_universal_destroy,
           ._params = SVG_param_list_new(),
           ._children = 0,
           ._chld_sz = 0,
           ._chld_len = 0,
           ._userdata = 0,
    };
 
    SVG_shape_add_child(shp, SVGSHP_content_new(content));
    struct SVG_string* style = SVG_string_new(0);
    SVG_string_assign(style, "font-family: monospace;");
 
    SVG_param_list_add(shp->_params, "x", SVG_INT, &x, sizeof x);
    SVG_param_list_add(shp->_params, "y", SVG_INT, &y, sizeof y);
    SVG_param_list_add(shp->_params, "font-size", SVG_INT, &font_size, sizeof font_size);
    SVG_param_list_add(shp->_params, "fill", SVG_STRING, fill, sizeof fill);
    SVG_param_list_add(shp->_params, "style", SVG_STRING, style, sizeof style);
 
    SVG_string_destroy(&style);
    return shp;
}
 
//////////////////////////////////// CIRCLE ////////////////////////////////////
int
SVGSHP_circle_start(struct SVG_shape* this, FILE* outp) {
    xfprintf(outp, "<circle ");
    SVG_param_list_print(this->_params, outp);
    xfprintf(outp, "/>");
    return 0;
}
 
struct SVG_shape*
SVGSHP_circle_new(float r, int cx, int cy,
                  struct SVG_string* stroke,
                  struct SVG_string* fill) {
    struct SVG_shape* shp = xmalloc(sizeof(struct SVG_shape));
    *shp = (struct SVG_shape) {
           ._start = &SVGSHP_circle_start,
           ._finish = &SVGSHP_nl_finish,
           ._destroy = &SVGSHP_universal_destroy,
           ._params = SVG_param_list_new(),
           ._children = 0,
           ._chld_sz = 0,
           ._chld_len = 0,
           ._userdata = 0,
    };
 
    SVG_param_list_add(shp->_params, "r", SVG_COORD, &r, sizeof r);
    SVG_param_list_add(shp->_params, "cx", SVG_INT, &cx, sizeof cx);
    SVG_param_list_add(shp->_params, "cy", SVG_INT, &cy, sizeof cy);
    SVG_param_list_add(shp->_params, "stroke", SVG_STRING, stroke, sizeof stroke);
    SVG_param_list_add(shp->_params, "fill", SVG_STRING, fill, sizeof fill);
    return shp;
}
 
///////////////////////////////////// LINE /////////////////////////////////////
int
SVGSHP_line_start(struct SVG_shape* this, FILE* outp) {
    xfprintf(outp, "<line ");
    SVG_param_list_print(this->_params, outp);
    xfprintf(outp, "/>");
    return 0;
}
 
struct SVG_shape*
SVGSHP_line_new(float x1, float y1,
                float x2, float y2,
                struct SVG_string* stroke) {
    struct SVG_shape* shp = xmalloc(sizeof(struct SVG_shape));
    *shp = (struct SVG_shape) {
           ._start = &SVGSHP_line_start,
           ._finish = &SVGSHP_nl_finish,
           ._destroy = &SVGSHP_universal_destroy,
           ._params = SVG_param_list_new(),
           ._children = 0,
           ._chld_sz = 0,
           ._chld_len = 0,
           ._userdata = 0,
    };
 
    SVG_param_list_add(shp->_params, "x1", SVG_COORD, &x1, sizeof x1);
    SVG_param_list_add(shp->_params, "y1", SVG_COORD, &y1, sizeof y1);
    SVG_param_list_add(shp->_params, "x2", SVG_COORD, &x2, sizeof x2);
    SVG_param_list_add(shp->_params, "y2", SVG_COORD, &y2, sizeof x1);
    SVG_param_list_add(shp->_params, "stroke", SVG_STRING, stroke, sizeof *stroke);
    return shp;
}
 
#define SVG_COLOR(name, value) \
    struct SVG_string* name = SVG_string_new(sizeof(value) - 1); \
    SVG_string_assign(name, value);
 
#define SVG_PI 3.141592f
 
void
clock_hour_ticks(struct SVG_shape* root,
                 float r, struct SVG_string* stroke) {
    const float section_rad = SVG_PI * 2 / 24;
    float tick_r = r - 20.f;
 
    for (int i = 0; i < 24; ++i) {
        SVG_shape_add_child(root,
                            SVGSHP_line_new(r * cosf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            r * sinf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            tick_r * cosf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            tick_r * sinf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            stroke));
    }
}
 
void
clock_min_ticks(struct SVG_shape* root,
                float r, struct SVG_string* stroke) {
    const float section_rad = SVG_PI * 2 / (24 * 5);
    float tick_r = r - 10.f;
 
    for (int i = 0; i < 24 * 5; ++i) {
        SVG_shape_add_child(root,
                            SVGSHP_line_new(r * cosf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            r * sinf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            tick_r * cosf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            tick_r * sinf(3 * SVG_PI / 2 + section_rad * (float) i) + r,
                                            stroke));
    }
}
 
void
clock_hand(struct SVG_shape* root,
           float pos, float r,
           float start_r, float end_r,
           struct SVG_string* color) {
    SVG_shape_add_child(root,
                        SVGSHP_line_new(start_r * cosf(pos + 3 * SVG_PI / 2) + r,
                                        start_r * sinf(pos + 3 * SVG_PI / 2) + r,
                                        end_r * cosf(pos + 3 * SVG_PI / 2) + r,
                                        end_r * sinf(pos + 3 * SVG_PI / 2) + r,
                                        color));
}
 
#define SVG_COLOR_PALE "77"
 
void
clock_hour_hand(struct SVG_shape* root,
                float pos, float r) {
    SVG_COLOR(clr_hour, "#FF7A93")
    SVG_COLOR(clr_hour_pale, "#FF7A93" SVG_COLOR_PALE)
 
    float start_r = 0.f;
    float end_r = r / 3.f;
    clock_hand(root, pos, r, start_r, end_r, clr_hour);
    clock_hand(root, pos, r, start_r, r, clr_hour_pale);
 
    SVG_string_destroy(&clr_hour);
    SVG_string_destroy(&clr_hour_pale);
}
 
void
clock_min_hand(struct SVG_shape* root,
               float pos, float r) {
    SVG_COLOR(clr_min, "#B9F27C")
    SVG_COLOR(clr_min_pale, "#B9F27C" SVG_COLOR_PALE)
 
    float start_r = r / 3.f;
    float end_r = 2.f * r / 3.f;
    clock_hand(root, pos, r, start_r, end_r, clr_min);
    clock_hand(root, pos, r, start_r, r, clr_min_pale);
 
    SVG_string_destroy(&clr_min);
    SVG_string_destroy(&clr_min_pale);
}
 
void
clock_sec_hand(struct SVG_shape* root,
               float pos, float r) {
    SVG_COLOR(clr_sec, "#AD8EE6")
    SVG_COLOR(clr_sec_pale, "#AD8EE6" SVG_COLOR_PALE)
 
    float start_r = 2.f * r / 3.f;
    float end_r = r;
    clock_hand(root, pos, r, start_r, end_r, clr_sec);
    clock_hand(root, pos, r, start_r, r, clr_sec_pale);
 
    SVG_string_destroy(&clr_sec);
    SVG_string_destroy(&clr_sec_pale);
}
 
int
main(void) {
    SVG_COLOR(clr_fg, "#A9B1D6")
    SVG_COLOR(clr_fg_pale, "#A9B1D6" SVG_COLOR_PALE)
    SVG_COLOR(clr_bg, "#20212E")
 
    FILE* clock = fopen("ora.svg", "w");
 
    const float R = 210.f;
    const float FontSize = 26.f;
 
    struct SVG_shape* root = SVGSHP_root_new((int) (2 * R), (int) (2 * R));
 
    SVG_shape_add_child(root, SVGSHP_circle_new(R, (int) R, (int) R, clr_fg, clr_bg));
    SVG_shape_add_child(root, SVGSHP_text_new((int) (R - (4.f / 3.f) * (FontSize - 2)),
                                              (int) (2 * FontSize),
                                              (int) FontSize,
                                              "XXIV",
                                              clr_fg));
 
    clock_hour_ticks(root, R, clr_fg);
    clock_min_ticks(root, R, clr_fg);
    float real_h, real_m, real_s;
    if (scanf("%f %f %f", &real_h, &real_m, &real_s) != 3) exit(2);
 
    float effective_h = real_h + real_m / 60.f + real_s / 60.f / 60.f;
    float effective_m = real_m + real_s / 60.f;
    float effective_s = real_s;
 
    clock_hour_hand(root, effective_h / 24.f * SVG_PI * 2, R);
    clock_min_hand(root, effective_m / 60.f * SVG_PI * 2, R);
    clock_sec_hand(root, effective_s / 60.f * SVG_PI * 2, R);
 
    SVG_print(root, clock);
 
    fclose(clock);
 
    MBR_CALL(root, destroy)(root);
    SVG_string_destroy(&clr_fg);
    SVG_string_destroy(&clr_fg_pale);
    SVG_string_destroy(&clr_bg);
}
