//
//  File: %sys-image.h
//  Summary: {Definitions for IMAGE! Datatype}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2019 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// See %src/extensions/image/README.md
//

enum {
    IDX_IMGDATA_BINARY,
    IDX_IMGDATA_WIDTH,
    IDX_IMGDATA_HEIGHT
};

inline static REBVAL *VAL_IMAGE_BIN(const REBCEL *v) {
    assert(REB_IMAGE == CELL_KIND(v));
    return KNOWN(ARR_AT(PAYLOAD(Image, v).details, IDX_IMGDATA_BINARY));
}

#define VAL_IMAGE_WIDE(v) \
    VAL_INT64(ARR_AT(PAYLOAD(Image, v).details, IDX_IMGDATA_WIDTH))

#define VAL_IMAGE_HIGH(v) \
    VAL_INT64(ARR_AT(PAYLOAD(Image, v).details, IDX_IMGDATA_HEIGHT))

inline static REBYTE *VAL_IMAGE_HEAD(const REBCEL *v) {
    assert(REB_IMAGE == CELL_KIND(v));
    return SER_DATA_RAW(VAL_BINARY(VAL_IMAGE_BIN(v)));
}

inline static REBYTE *VAL_IMAGE_AT_HEAD(const REBCEL *v, REBCNT pos) {
    return VAL_IMAGE_HEAD(v) + (pos * 4);
}


// !!! The functions that take into account the current index position in the
// IMAGE!'s ANY-SERIES! payload are sketchy, in the sense that being offset
// into the data does not change the width or height...only the length when
// viewing the image as a 1-dimensional series.  This is not likely to make
// a lot of sense.

#define VAL_IMAGE_POS(v) \
    VAL_INDEX(VAL_IMAGE_BIN(v))

inline static REBYTE *VAL_IMAGE_AT(const REBCEL *v) {
    return VAL_IMAGE_AT_HEAD(v, VAL_IMAGE_POS(v));
}

inline static REBCNT VAL_IMAGE_LEN_HEAD(const REBCEL *v) {
    return VAL_IMAGE_HIGH(v) * VAL_IMAGE_WIDE(v);
}

inline static REBCNT VAL_IMAGE_LEN_AT(const REBCEL *v) {
    if (VAL_IMAGE_POS(v) >= VAL_IMAGE_LEN_HEAD(v))
        return 0;  // avoid negative position
    return VAL_IMAGE_LEN_HEAD(v) - VAL_IMAGE_POS(v);
}

inline static REBVAL *Init_Image(
    RELVAL *out,
    REBSER *bin,
    REBCNT wide,
    REBCNT high
){
    assert(GET_SERIES_FLAG(bin, MANAGED));

    REBARR *a = Make_Arr_Core(3, NODE_FLAG_MANAGED);
    Init_Binary(ARR_AT(a, IDX_IMGDATA_BINARY), bin);
    Init_Integer(ARR_AT(a, IDX_IMGDATA_WIDTH), wide);
    Init_Integer(ARR_AT(a, IDX_IMGDATA_HEIGHT), high);
    TERM_ARRAY_LEN(a, 3);

    RESET_CELL(out, REB_IMAGE);
    PAYLOAD(Image, out).details = a;

    assert(VAL_IMAGE_POS(out) == 0);  // !!! sketchy concept, is in BINARY!

    return KNOWN(out);
}


inline static void RESET_IMAGE(REBYTE *p, REBCNT num_pixels) {
    REBYTE *start = p;
    REBYTE *stop = start + (num_pixels * 4);
    while (start < stop) {
        *start++ = 0; // red
        *start++ = 0; // green
        *start++ = 0; // blue
        *start++ = 0xff; // opaque alpha, R=G=B as 0 means black pixel
    }
}

// Creates WxH image, black pixels, all opaque.
//
inline static REBVAL *Init_Image_Black_Opaque(RELVAL *out, REBCNT w, REBCNT h)
{
    REBSIZ size = (w * h) * 4;  // RGBA pixels, 4 bytes each
    REBBIN *bin = Make_Binary(size);
    SET_SERIES_LEN(bin, size);
    TERM_SERIES(bin);
    MANAGE_SERIES(bin);

    RESET_IMAGE(SER_DATA_RAW(bin), (w * h)); // length in 'pixels'

    return Init_Image(out, bin, w, h);
}


// !!! These hooks allow the REB_IMAGE cell type to dispatch to code in the
// IMAGE! extension if it is loaded.
//
extern REBINT CT_Image(const REBCEL *a, const REBCEL *b, REBINT mode);
extern REB_R MAKE_Image(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg);
extern REB_R TO_Image(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg);
extern void MF_Image(REB_MOLD *mo, const REBCEL *v, bool form);
extern REBTYPE(Image);
extern REB_R PD_Image(REBPVS *pvs, const REBVAL *picker, const REBVAL *opt_setval);